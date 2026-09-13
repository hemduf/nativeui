#pragma once

#include <nativeui/dispatcher.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Exact cubic easing functions supported by NativeUI v1 animations.
enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

/// Explicit retained invalidation policy attached to every animation.
enum class AnimationInvalidation {
    Paint,
    Layout,
};

/// Parameters for the v1 semi-implicit damped spring solver.
struct SpringOptions {
    float stiffness{170.0f};
    float damping{26.0f};
    float initial_velocity{0.0f};
    float distance_epsilon{0.001f};
    float velocity_epsilon{0.001f};
    DispatcherDuration max_dt{1.0 / 30.0};
};

/// Opaque active-animation identity scoped to exactly one AnimationContext.
class AnimationHandle final {
public:
    AnimationHandle() = default;

    [[nodiscard]] bool valid() const noexcept { return owner_ && id_ != 0; }
    explicit operator bool() const noexcept { return valid(); }

    friend bool operator==(const AnimationHandle&, const AnimationHandle&) noexcept = default;

private:
    AnimationHandle(std::shared_ptr<const void> owner, std::uint64_t id) noexcept
        : owner_(std::move(owner)), id_(id) {}

    std::shared_ptr<const void> owner_;
    std::uint64_t id_{};

    friend class AnimationContext;
};

inline const AnimationHandle kInvalidAnimationHandle{};

/// One instance-owned animation registry for one UI/view context.
///
/// AnimationContext is UI-thread confined. It owns no thread or OS timer: while
/// at least one animation is active it arms at most one one-shot T065 timer and
/// rearms that timer at the fixed 16 ms wake cadence after each callback. Tween
/// and spring math use the owning Dispatcher's injected monotonic clock, so
/// delayed callbacks use real elapsed time without replaying hidden substeps.
///
/// The caller supplies the retained write and invalidation callbacks. This keeps
/// animation policy independent from a specific Component while allowing Paint
/// invalidation to remain bounded to the caller's owner/region.
class AnimationContext final {
public:
    using ValueCallback = std::function<void(float)>;
    using InvalidationCallback = std::function<void(AnimationInvalidation)>;
    using CompletionCallback = std::function<void()>;

    explicit AnimationContext(Dispatcher dispatcher)
        : state_(std::make_shared<State>(std::move(dispatcher))) {}

    ~AnimationContext() { shutdown(state_); }

    AnimationContext(const AnimationContext&) = delete;
    AnimationContext& operator=(const AnimationContext&) = delete;
    AnimationContext(AnimationContext&&) = delete;
    AnimationContext& operator=(AnimationContext&&) = delete;

    [[nodiscard]] AnimationHandle start_tween(
        float from,
        float to,
        DispatcherDuration duration,
        Easing easing,
        AnimationInvalidation invalidation,
        ValueCallback write,
        InvalidationCallback invalidate = {},
        CompletionCallback completion = {}) {
        const auto state = state_;
        if (!state || state->closing || !state->dispatcher.valid() || !write ||
            !valid_tween(from, to, duration, easing)) {
            return {};
        }

        if (duration == DispatcherDuration::zero() || state->reduced_motion) {
            apply_immediate(state, to, invalidation, std::move(write),
                            std::move(invalidate), std::move(completion));
            return {};
        }

        if (state->next_id == 0) return {};
        const auto now = state->dispatcher.current_time();
        const auto id = state->next_id++;
        Entry entry;
        entry.id = id;
        entry.kind = EntryKind::Tween;
        entry.from = from;
        entry.value = from;
        entry.target = to;
        entry.duration = duration;
        entry.easing = easing;
        entry.start_time = now;
        entry.previous_time = now;
        entry.invalidation = invalidation;
        entry.write = std::move(write);
        entry.invalidate = std::move(invalidate);
        entry.completion = std::move(completion);
        state->entries.push_back(std::move(entry));

        const AnimationHandle handle{state->token, id};
        if (!ensure_wake(state)) {
            erase_entry(state, id);
            return {};
        }
        return handle;
    }

    [[nodiscard]] AnimationHandle start_spring(
        float value,
        float target,
        SpringOptions options,
        AnimationInvalidation invalidation,
        ValueCallback write,
        InvalidationCallback invalidate = {},
        CompletionCallback completion = {}) {
        const auto state = state_;
        if (!state || state->closing || !state->dispatcher.valid() || !write ||
            !valid_spring(value, target, options)) {
            return {};
        }

        if (state->reduced_motion) {
            apply_immediate(state, target, invalidation, std::move(write),
                            std::move(invalidate), std::move(completion));
            return {};
        }

        if (state->next_id == 0) return {};
        const auto now = state->dispatcher.current_time();
        const auto id = state->next_id++;
        Entry entry;
        entry.id = id;
        entry.kind = EntryKind::Spring;
        entry.value = value;
        entry.target = target;
        entry.velocity = options.initial_velocity;
        entry.spring = options;
        entry.start_time = now;
        entry.previous_time = now;
        entry.invalidation = invalidation;
        entry.write = std::move(write);
        entry.invalidate = std::move(invalidate);
        entry.completion = std::move(completion);
        state->entries.push_back(std::move(entry));

        const AnimationHandle handle{state->token, id};
        if (!ensure_wake(state)) {
            erase_entry(state, id);
            return {};
        }
        return handle;
    }

    [[nodiscard]] bool cancel(const AnimationHandle& handle) noexcept {
        const auto state = state_;
        if (!state || state->closing || !handle.valid() ||
            handle.owner_.get() != state->token.get()) {
            return false;
        }
        if (!erase_entry(state, handle.id_)) return false;
        cancel_idle_wake(state);
        return true;
    }

    void set_reduced_motion(bool enabled) {
        const auto state = state_;
        if (!state || state->closing || state->reduced_motion == enabled) return;
        state->reduced_motion = enabled;
        if (!enabled) {
            if (!state->entries.empty() && !ensure_wake(state)) state->entries.clear();
            return;
        }

        if (state->wake.valid()) {
            (void)state->dispatcher.cancel(state->wake);
            state->wake = {};
        }

        std::vector<std::uint64_t> ids;
        ids.reserve(state->entries.size());
        for (const auto& entry : state->entries) ids.push_back(entry.id);

        for (const auto id : ids) {
            auto* entry = find_entry(state, id);
            if (!entry) continue;
            const float target = entry->target;
            const auto invalidation = entry->invalidation;
            auto write = entry->write;
            auto invalidate = entry->invalidate;
            auto completion = entry->completion;
            entry->value = target;
            entry->velocity = 0.0f;

            write(target);
            if (state->closing) return;
            if (invalidate) invalidate(invalidation);
            if (state->closing) return;

            if (!erase_entry(state, id)) continue;
            if (completion) completion();
            if (state->closing) return;
        }
    }

    [[nodiscard]] bool reduced_motion() const noexcept {
        const auto state = state_;
        return state && !state->closing && state->reduced_motion;
    }

    [[nodiscard]] std::size_t active_count() const noexcept {
        const auto state = state_;
        return state && !state->closing ? state->entries.size() : 0;
    }

private:
    enum class EntryKind {
        Tween,
        Spring,
    };

    struct Entry {
        std::uint64_t id{};
        EntryKind kind{EntryKind::Tween};
        float from{};
        float value{};
        float target{};
        float velocity{};
        DispatcherDuration duration{};
        Easing easing{Easing::Linear};
        SpringOptions spring{};
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point previous_time{};
        AnimationInvalidation invalidation{AnimationInvalidation::Paint};
        ValueCallback write;
        InvalidationCallback invalidate;
        CompletionCallback completion;
    };

    struct State {
        explicit State(Dispatcher owner_dispatcher)
            : dispatcher(std::move(owner_dispatcher)), token(std::make_shared<int>(0)) {}

        Dispatcher dispatcher;
        std::shared_ptr<const void> token;
        std::vector<Entry> entries;
        TimerHandle wake;
        std::uint64_t next_id{1};
        bool reduced_motion{};
        bool closing{};
    };

    inline static constexpr DispatcherDuration kWakeCadence{0.016};

    [[nodiscard]] static bool finite(float value) noexcept {
        return std::isfinite(value);
    }

    [[nodiscard]] static bool valid_easing(Easing easing) noexcept {
        switch (easing) {
        case Easing::Linear:
        case Easing::EaseIn:
        case Easing::EaseOut:
        case Easing::EaseInOut:
            return true;
        }
        return false;
    }

    [[nodiscard]] static bool valid_tween(float from,
                                          float to,
                                          DispatcherDuration duration,
                                          Easing easing) noexcept {
        return finite(from) && finite(to) && std::isfinite(duration.count()) &&
            duration >= DispatcherDuration::zero() && valid_easing(easing);
    }

    [[nodiscard]] static bool valid_spring(float value,
                                           float target,
                                           const SpringOptions& options) noexcept {
        return finite(value) && finite(target) && finite(options.stiffness) &&
            finite(options.damping) && finite(options.initial_velocity) &&
            finite(options.distance_epsilon) && finite(options.velocity_epsilon) &&
            std::isfinite(options.max_dt.count()) && options.stiffness >= 0.0f &&
            options.damping >= 0.0f && options.distance_epsilon >= 0.0f &&
            options.velocity_epsilon >= 0.0f && options.max_dt > DispatcherDuration::zero();
    }

    [[nodiscard]] static float easing_value(Easing easing, float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (easing) {
        case Easing::Linear:
            return t;
        case Easing::EaseIn:
            return t * t * t;
        case Easing::EaseOut: {
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case Easing::EaseInOut:
            if (t < 0.5f) return 4.0f * t * t * t;
            {
                const float u = -2.0f * t + 2.0f;
                return 1.0f - (u * u * u) * 0.5f;
            }
        }
        return t;
    }

    [[nodiscard]] static Entry* find_entry(const std::shared_ptr<State>& state,
                                            std::uint64_t id) noexcept {
        const auto it = std::find_if(state->entries.begin(), state->entries.end(),
                                     [id](const Entry& entry) { return entry.id == id; });
        return it == state->entries.end() ? nullptr : &*it;
    }

    [[nodiscard]] static bool erase_entry(const std::shared_ptr<State>& state,
                                          std::uint64_t id) noexcept {
        const auto it = std::find_if(state->entries.begin(), state->entries.end(),
                                     [id](const Entry& entry) { return entry.id == id; });
        if (it == state->entries.end()) return false;
        state->entries.erase(it);
        return true;
    }

    static void cancel_idle_wake(const std::shared_ptr<State>& state) noexcept {
        if (!state->entries.empty() || !state->wake.valid()) return;
        (void)state->dispatcher.cancel(state->wake);
        state->wake = {};
    }

    [[nodiscard]] static bool ensure_wake(const std::shared_ptr<State>& state) {
        if (state->closing || state->entries.empty() || state->reduced_motion) return true;
        if (state->wake.valid()) return true;

        const std::weak_ptr<State> weak_state{state};
        auto handle = state->dispatcher.schedule_after(kWakeCadence, [weak_state] {
            const auto locked = weak_state.lock();
            if (!locked || locked->closing) return;
            locked->wake = {};
            tick(locked);
        });
        if (!handle.valid()) return false;
        state->wake = std::move(handle);
        return true;
    }

    static void apply_immediate(const std::shared_ptr<State>& state,
                                float target,
                                AnimationInvalidation invalidation,
                                ValueCallback write,
                                InvalidationCallback invalidate,
                                CompletionCallback completion) {
        write(target);
        if (state->closing) return;
        if (invalidate) invalidate(invalidation);
        if (state->closing) return;
        if (completion) completion();
    }

    static void tick(const std::shared_ptr<State>& state) {
        if (state->closing || state->reduced_motion) return;
        const auto now = state->dispatcher.current_time();

        std::vector<std::uint64_t> ids;
        ids.reserve(state->entries.size());
        for (const auto& entry : state->entries) ids.push_back(entry.id);

        for (const auto id : ids) {
            auto* entry = find_entry(state, id);
            if (!entry) continue;

            float next_value = entry->value;
            bool completed = false;
            if (entry->kind == EntryKind::Tween) {
                const double elapsed = std::max(
                    0.0,
                    std::chrono::duration_cast<DispatcherDuration>(now - entry->start_time).count());
                const double duration = entry->duration.count();
                const float t = duration > 0.0
                    ? static_cast<float>(std::clamp(elapsed / duration, 0.0, 1.0))
                    : 1.0f;
                completed = t >= 1.0f;
                next_value = completed
                    ? entry->target
                    : entry->from + (entry->target - entry->from) * easing_value(entry->easing, t);
            } else {
                const double elapsed = std::max(
                    0.0,
                    std::chrono::duration_cast<DispatcherDuration>(now - entry->previous_time).count());
                const float dt = static_cast<float>(
                    std::min(elapsed, entry->spring.max_dt.count()));
                const float acceleration =
                    entry->spring.stiffness * (entry->target - entry->value) -
                    entry->spring.damping * entry->velocity;
                entry->velocity += acceleration * dt;
                next_value = entry->value + entry->velocity * dt;
                if (std::fabs(entry->target - next_value) <= entry->spring.distance_epsilon &&
                    std::fabs(entry->velocity) <= entry->spring.velocity_epsilon) {
                    next_value = entry->target;
                    entry->velocity = 0.0f;
                    completed = true;
                }
                entry->previous_time = now;
            }

            entry->value = next_value;
            const auto invalidation = entry->invalidation;
            auto write = entry->write;
            auto invalidate = entry->invalidate;
            auto completion = entry->completion;

            write(next_value);
            if (state->closing) return;
            if (invalidate) invalidate(invalidation);
            if (state->closing) return;

            if (!completed || !find_entry(state, id)) continue;
            (void)erase_entry(state, id);
            if (completion) completion();
            if (state->closing) return;
        }

        if (!state->entries.empty() && !ensure_wake(state)) {
            // The owning dispatcher can become unavailable while callbacks run.
            // Treat unschedulable residual entries as cancelled: never retain a
            // logically active animation that has no future wake source.
            state->entries.clear();
        }
    }

    static void shutdown(const std::shared_ptr<State>& state) noexcept {
        if (!state || state->closing) return;
        state->closing = true;
        if (state->wake.valid()) {
            (void)state->dispatcher.cancel(state->wake);
            state->wake = {};
        }
        state->entries.clear();
    }

    std::shared_ptr<State> state_;
};

} // namespace ui
