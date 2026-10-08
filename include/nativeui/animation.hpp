#pragma once

#include <nativeui/dispatcher.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Easing curve used by a finite-duration tween.
///
/// The easing function maps normalized elapsed time in [0, 1] to interpolation
/// progress. All non-linear variants are cubic and are evaluated from elapsed
/// Dispatcher time; they do not depend on the number of wake callbacks.
enum class Easing {
    /// Constant-rate interpolation: f(t) = t.
    Linear,
    /// Cubic acceleration from rest: f(t) = t^3.
    EaseIn,
    /// Cubic deceleration into the target.
    EaseOut,
    /// Symmetric cubic ease-in/ease-out around t = 0.5.
    EaseInOut,
};

/// Retained invalidation class requested after an animation writes a value.
enum class AnimationInvalidation {
    /// Repaint the target without declaring layout geometry dirty.
    Paint,
    /// Re-run retained layout before repainting the target.
    Layout,
};

/// Parameters for the scalar spring integrator used by start_spring().
///
/// The animated scalar is unit-agnostic. If its unit is U, velocity is U/s,
/// stiffness acts as s^-2 and damping as s^-1 in the implemented equation
///     acceleration = stiffness * (target - value) - damping * velocity.
///
/// All fields must be finite. stiffness, damping and both epsilons must be
/// non-negative; max_dt must be strictly positive. Invalid options cause
/// start_spring() to return an empty handle without invoking callbacks.
struct SpringOptions {
    /// Restoring-force coefficient in s^-2. Zero is allowed.
    float stiffness{170.0f};
    /// Velocity damping coefficient in s^-1. Zero is allowed.
    float damping{26.0f};
    /// Initial scalar velocity in animated-units per second.
    float initial_velocity{0.0f};
    /// Maximum |target - value| that can satisfy the rest condition, in U.
    float distance_epsilon{0.001f};
    /// Maximum |velocity| that can satisfy the rest condition, in U/s.
    float velocity_epsilon{0.001f};
    /// Maximum elapsed time consumed by one spring integration step.
    ///
    /// DispatcherDuration is seconds. A delayed wake performs one step clamped
    /// to this duration; the solver does not replay hidden catch-up substeps.
    DispatcherDuration max_dt{1.0 / 30.0};
};

/// Owned retained invalidation routes for one animation target.
///
/// The paint and layout callbacks are stored by value. NativeUI does not infer
/// ownership from their captures: callers must ensure any borrowed state captured
/// by custom callbacks remains safe for every animation step. Retained component
/// owners should pass their lifetime-safe node-bounded paint invalidator and
/// ancestor layout invalidator.
///
/// Both callbacks are required for a valid target even when one animation uses
/// only one invalidation class. Invoking a route may run application/framework
/// code and may throw; AnimationContext applies its normal step-failure recovery.
class AnimationInvalidationTarget final {
public:
    /// Construct an invalid target with no routes.
    AnimationInvalidationTarget() = default;

    /// Store the paint and layout invalidation routes.
    ///
    /// The std::function objects are owned by the target. Construction/copying
    /// can allocate and is not an audio-real-time operation.
    AnimationInvalidationTarget(std::function<void()> paint_invalidator,
                                std::function<void()> layout_invalidator)
        : paint_(std::move(paint_invalidator)),
          layout_(std::move(layout_invalidator)) {}

    /// True only when both paint and layout routes are non-empty.
    [[nodiscard]] bool valid() const noexcept {
        return static_cast<bool>(paint_) && static_cast<bool>(layout_);
    }

private:
    void invalidate(AnimationInvalidation kind) const {
        if (kind == AnimationInvalidation::Layout) {
            layout_();
        } else {
            paint_();
        }
    }

    std::function<void()> paint_;
    std::function<void()> layout_;

    friend class AnimationContext;
};

/// Opaque identity for one scheduled animation in one AnimationContext.
///
/// A handle retains only a small owner token, not the AnimationContext, its
/// Dispatcher queue, target callbacks or animated object. Therefore keeping a
/// handle never keeps an animation alive. valid() means "non-empty identity",
/// not "currently active": a completed, cancelled or owner-destroyed handle can
/// remain syntactically valid and cancel() will then return false.
class AnimationHandle final {
public:
    /// Construct the canonical empty handle.
    AnimationHandle() = default;

    /// Return whether this handle contains an owner identity and non-zero id.
    [[nodiscard]] bool valid() const noexcept { return owner_ && id_ != 0; }
    /// Equivalent to valid(); does not query whether the animation is active.
    explicit operator bool() const noexcept { return valid(); }

    /// Compare the complete owner-token/id identity.
    friend bool operator==(const AnimationHandle&, const AnimationHandle&) noexcept = default;

private:
    AnimationHandle(std::shared_ptr<const void> owner, std::uint64_t id) noexcept
        : owner_(std::move(owner)), id_(id) {}

    std::shared_ptr<const void> owner_;
    std::uint64_t id_{};

    friend class AnimationContext;
};

/// Canonical empty animation handle.
inline const AnimationHandle kInvalidAnimationHandle{};

/// Instance-owned scalar animation registry for one UI/view domain.
///
/// AnimationContext is UI/main-thread confined and is not internally
/// synchronized. It owns no worker thread or independent event loop: while at
/// least one animation is active it coalesces work onto at most one one-shot
/// Dispatcher timer, normally rearmed at a 16 ms wake cadence. Elapsed-time math
/// uses the owning Dispatcher's monotonic clock, so tween progress reflects real
/// elapsed time even when event-loop checkpoints are delayed.
///
/// Value callbacks, invalidation callbacks and completion callbacks execute
/// synchronously on the Dispatcher's owner/UI thread. They may re-enter the
/// AnimationContext, cancel animations, start new animations, or destroy the
/// context. Destruction is terminal and callback-silent.
///
/// All scheduling, callback storage and per-tick snapshotting may allocate;
/// AnimationContext is never an audio/DSP real-time primitive.
class AnimationContext final {
public:
    /// Receives the scalar value produced by one animation step.
    ///
    /// For a non-zero-duration tween, start_tween() does not synchronously emit
    /// the initial `from` value; the first write occurs on a later wake.
    using ValueCallback = std::function<void(float)>;
    /// Invoked once after the exact target write and retained invalidation commit.
    ///
    /// Cancellation never invokes completion.
    using CompletionCallback = std::function<void()>;

    /// Bind this context to one Dispatcher owner.
    ///
    /// The Dispatcher is a weak owner handle; constructing the AnimationContext
    /// does not keep its UI/event loop alive. An invalid Dispatcher is accepted,
    /// but start operations then fail with an empty handle. State allocation may
    /// throw. The context must subsequently be used from its owner/UI thread.
    explicit AnimationContext(Dispatcher dispatcher)
        : state_(std::make_shared<State>(std::move(dispatcher))) {}

    /// Cancel the pending wake and discard all animations without user callbacks.
    ~AnimationContext() { shutdown(state_); }

    AnimationContext(const AnimationContext&) = delete;
    AnimationContext& operator=(const AnimationContext&) = delete;
    AnimationContext(AnimationContext&&) = delete;
    AnimationContext& operator=(AnimationContext&&) = delete;

    /// Start a finite-duration scalar tween.
    ///
    /// @param from Initial scalar used by interpolation. Must be finite.
    /// @param to Exact target scalar written on successful completion. Must be finite.
    /// @param duration Tween duration in seconds. Must be finite and >= 0.
    /// @param easing Cubic/linear easing profile; unknown enum values are rejected.
    /// @param invalidation Retained invalidation route applied after every write.
    /// @param target Owned paint/layout invalidation routes; both must be valid.
    /// @param write Required callback receiving each produced scalar value.
    /// @param completion Optional callback after final write + invalidation.
    /// @return A context-scoped active handle, or an empty handle on validation,
    ///         dispatcher/scheduling, id-exhaustion, or immediate-completion paths.
    ///
    /// duration == 0 and reduced-motion mode are synchronous immediate paths:
    /// `write(to)`, the selected invalidation route, then `completion()` run
    /// before this function returns, and no active handle/timer is created.
    ///
    /// For a positive duration, interpolation is based on monotonic elapsed time;
    /// the final successful step writes `to` exactly. Failure to obtain the
    /// first Dispatcher wake removes the provisional entry. Allocation/scheduler
    /// exceptions propagate after cleanup.
    [[nodiscard]] AnimationHandle start_tween(
        float from,
        float to,
        DispatcherDuration duration,
        Easing easing,
        AnimationInvalidation invalidation,
        AnimationInvalidationTarget target,
        ValueCallback write,
        CompletionCallback completion = {}) {
        const auto state = state_;
        if (!state || state->closing || !state->dispatcher.valid() || !write ||
            !target.valid() || !valid_tween(from, to, duration, easing)) {
            return {};
        }

        if (duration == DispatcherDuration::zero() || state->reduced_motion) {
            apply_immediate(state, to, invalidation, std::move(target),
                            std::move(write), std::move(completion));
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
        entry.target_invalidation = std::move(target);
        entry.write = std::move(write);
        entry.completion = std::move(completion);
        state->entries.push_back(std::move(entry));

        const AnimationHandle handle{state->token, id};
        try {
            if (!ensure_wake(state)) {
                (void)erase_entry(state, id);
                return {};
            }
        } catch (...) {
            (void)erase_entry(state, id);
            cancel_idle_wake(state);
            throw;
        }
        return handle;
    }

    /// Start a damped scalar spring toward `target`.
    ///
    /// @param value Initial scalar position, in application-defined units U.
    /// @param target Exact target scalar used by the spring/rest test.
    /// @param options Solver coefficients, initial velocity, rest epsilons and
    ///                maximum per-wake integration duration.
    /// @param invalidation Retained invalidation route applied after every write.
    /// @param invalidation_target Owned paint/layout invalidation callbacks.
    /// @param write Required callback receiving each produced scalar value.
    /// @param completion Optional callback after an exact target snap.
    /// @return A context-scoped active handle, or empty on invalid arguments,
    ///         unavailable scheduling/id, or reduced-motion immediate completion.
    ///
    /// The solver uses one semi-implicit Euler update per Dispatcher wake:
    /// velocity is updated from spring acceleration, then value from that new
    /// velocity. Elapsed time is clamped to options.max_dt; no catch-up substeps
    /// are replayed after a long stall. Completion requires both distance and
    /// velocity to be within their epsilons, then writes the target exactly.
    ///
    /// A physically non-converging valid configuration (for example zero
    /// stiffness away from the target with zero velocity) remains active until
    /// cancelled. In reduced-motion mode the target is applied synchronously and
    /// no handle is returned.
    [[nodiscard]] AnimationHandle start_spring(
        float value,
        float target,
        SpringOptions options,
        AnimationInvalidation invalidation,
        AnimationInvalidationTarget invalidation_target,
        ValueCallback write,
        CompletionCallback completion = {}) {
        const auto state = state_;
        if (!state || state->closing || !state->dispatcher.valid() || !write ||
            !invalidation_target.valid() || !valid_spring(value, target, options)) {
            return {};
        }

        if (state->reduced_motion) {
            apply_immediate(state, target, invalidation,
                            std::move(invalidation_target), std::move(write),
                            std::move(completion));
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
        entry.target_invalidation = std::move(invalidation_target);
        entry.write = std::move(write);
        entry.completion = std::move(completion);
        state->entries.push_back(std::move(entry));

        const AnimationHandle handle{state->token, id};
        try {
            if (!ensure_wake(state)) {
                (void)erase_entry(state, id);
                return {};
            }
        } catch (...) {
            (void)erase_entry(state, id);
            cancel_idle_wake(state);
            throw;
        }
        return handle;
    }

    /// Cancel one active animation owned by this context.
    ///
    /// Returns false for empty, stale, already-completed/already-cancelled and
    /// cross-context handles, or while this context is closing. Cancellation is
    /// noexcept, does not write another value and never invokes completion. If it
    /// removes the final active entry, the shared wake timer is cancelled best
    /// effort.
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

    /// Enable or disable reduced-motion behavior for this context.
    ///
    /// Enabling reduced motion cancels the pending wake and synchronously drives
    /// every currently active animation to its exact target in snapshot order:
    /// write -> retained invalidation -> erase entry -> completion. Callbacks may
    /// re-enter or destroy the context.
    ///
    /// If any allocation or user/invalidation/completion callback throws while
    /// enabling, reduced motion remains enabled, all remaining entries are
    /// discarded, the wake is cancelled, and the exception propagates. This
    /// prevents partially scheduled animation work from surviving the failed
    /// policy transition.
    ///
    /// While reduced motion is enabled, new tweens/springs take their synchronous
    /// immediate path and return an empty handle. Disabling the flag does not
    /// resurrect animations that already completed or were discarded.
    void set_reduced_motion(bool enabled) {
        const auto state = state_;
        if (!state || state->closing || state->reduced_motion == enabled) return;
        state->reduced_motion = enabled;
        if (!enabled) {
            if (!state->entries.empty()) {
                try {
                    if (!ensure_wake(state)) state->entries.clear();
                } catch (...) {
                    state->entries.clear();
                    cancel_wake_noexcept(state);
                    throw;
                }
            }
            return;
        }

        cancel_wake_noexcept(state);

        try {
            std::vector<std::uint64_t> ids;
            ids.reserve(state->entries.size());
            for (const auto& entry : state->entries) ids.push_back(entry.id);

            for (const auto id : ids) {
                auto* entry = find_entry(state, id);
                if (!entry) continue;
                const float target = entry->target;
                const auto invalidation = entry->invalidation;
                auto target_invalidation = entry->target_invalidation;
                auto write = entry->write;
                auto completion = entry->completion;
                entry->value = target;
                entry->velocity = 0.0f;

                write(target);
                if (state->closing) return;
                target_invalidation.invalidate(invalidation);
                if (state->closing) return;

                if (!erase_entry(state, id)) continue;
                if (completion) completion();
                if (state->closing) return;
            }

            if (state->entries.empty()) {
                cancel_idle_wake(state);
            } else if (!state->reduced_motion) {
                if (!ensure_wake(state)) state->entries.clear();
            }
        } catch (...) {
            // The outer request to enable reduced motion owns the failure
            // contract: no partially completed entry or reentrant wake survives.
            state->reduced_motion = true;
            state->entries.clear();
            cancel_wake_noexcept(state);
            throw;
        }
    }

    /// Return the current reduced-motion policy while the context is alive.
    [[nodiscard]] bool reduced_motion() const noexcept {
        const auto state = state_;
        return state && !state->closing && state->reduced_motion;
    }

    /// Return the number of currently active tween/spring entries.
    ///
    /// Immediate reduced-motion/zero-duration transitions are never counted.
    [[nodiscard]] std::size_t active_count() const noexcept {
        const auto state = state_;
        return state && !state->closing ? state->entries.size() : 0;
    }

private:
    enum class EntryKind { Tween, Spring };

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
        AnimationInvalidationTarget target_invalidation;
        ValueCallback write;
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

    [[nodiscard]] static bool finite(float value) noexcept { return std::isfinite(value); }

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

    [[nodiscard]] static bool valid_tween(float from, float to,
                                          DispatcherDuration duration,
                                          Easing easing) noexcept {
        return finite(from) && finite(to) && std::isfinite(duration.count()) &&
            duration >= DispatcherDuration::zero() && valid_easing(easing);
    }

    [[nodiscard]] static bool valid_spring(float value, float target,
                                           const SpringOptions& options) noexcept {
        return finite(value) && finite(target) && finite(options.stiffness) &&
            finite(options.damping) && finite(options.initial_velocity) &&
            finite(options.distance_epsilon) && finite(options.velocity_epsilon) &&
            std::isfinite(options.max_dt.count()) && options.stiffness >= 0.0f &&
            options.damping >= 0.0f && options.distance_epsilon >= 0.0f &&
            options.velocity_epsilon >= 0.0f &&
            options.max_dt > DispatcherDuration::zero();
    }

    [[nodiscard]] static float easing_value(Easing easing, float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (easing) {
        case Easing::Linear: return t;
        case Easing::EaseIn: return t * t * t;
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

    static void cancel_wake_noexcept(const std::shared_ptr<State>& state) noexcept {
        if (!state->wake.valid()) return;
        auto wake = std::move(state->wake);
        state->wake = {};
        try {
            (void)state->dispatcher.cancel(wake);
        } catch (...) {
            // Cleanup/destruction paths are terminal and no-throw. If the
            // Dispatcher itself cannot cancel, the weak timer callback still
            // observes closing/reduced/empty state and cannot invoke user code.
        }
    }

    static void cancel_idle_wake(const std::shared_ptr<State>& state) noexcept {
        if (!state->entries.empty()) return;
        cancel_wake_noexcept(state);
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

    static void recover_after_step_failure(const std::shared_ptr<State>& state,
                                           std::uint64_t id) noexcept {
        if (state->closing) return;

        // The failing step has begun. It is terminal and is never retried.
        (void)erase_entry(state, id);
        if (state->reduced_motion) {
            state->entries.clear();
            cancel_wake_noexcept(state);
            return;
        }
        if (state->entries.empty()) {
            cancel_idle_wake(state);
            return;
        }

        try {
            if (!ensure_wake(state)) {
                state->entries.clear();
                cancel_wake_noexcept(state);
            }
        } catch (...) {
            // Preserve the original application/framework exception. If the
            // recovery wake itself cannot be scheduled, terminalize the rest so
            // no logically active animation is left permanently unscheduled.
            state->entries.clear();
            cancel_wake_noexcept(state);
        }
    }

    static void recover_snapshot_failure(const std::shared_ptr<State>& state) noexcept {
        if (state->closing || state->entries.empty()) return;
        if (state->reduced_motion) {
            state->entries.clear();
            cancel_wake_noexcept(state);
            return;
        }
        try {
            if (!ensure_wake(state)) {
                state->entries.clear();
                cancel_wake_noexcept(state);
            }
        } catch (...) {
            state->entries.clear();
            cancel_wake_noexcept(state);
        }
    }

    static void apply_immediate(const std::shared_ptr<State>& state, float target,
                                AnimationInvalidation invalidation,
                                AnimationInvalidationTarget target_invalidation,
                                ValueCallback write,
                                CompletionCallback completion) {
        write(target);
        if (state->closing) return;
        target_invalidation.invalidate(invalidation);
        if (state->closing) return;
        if (completion) completion();
    }

    static void tick(const std::shared_ptr<State>& state) {
        if (state->closing || state->reduced_motion) return;
        const auto now = state->dispatcher.current_time();

        std::vector<std::uint64_t> ids;
        try {
            ids.reserve(state->entries.size());
            for (const auto& entry : state->entries) ids.push_back(entry.id);
        } catch (...) {
            recover_snapshot_failure(state);
            throw;
        }

        for (const auto id : ids) {
            if (!find_entry(state, id)) continue;

            try {
                auto* entry = find_entry(state, id);
                if (!entry) continue;

                float next_value = entry->value;
                bool completed = false;
                if (entry->kind == EntryKind::Tween) {
                    const double elapsed = std::max(
                        0.0, std::chrono::duration_cast<DispatcherDuration>(
                                 now - entry->start_time).count());
                    const double duration = entry->duration.count();
                    const float t = duration > 0.0
                        ? static_cast<float>(std::clamp(elapsed / duration, 0.0, 1.0))
                        : 1.0f;
                    completed = t >= 1.0f;
                    next_value = completed
                        ? entry->target
                        : entry->from + (entry->target - entry->from) *
                              easing_value(entry->easing, t);
                } else {
                    const double elapsed = std::max(
                        0.0, std::chrono::duration_cast<DispatcherDuration>(
                                 now - entry->previous_time).count());
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
                auto target_invalidation = entry->target_invalidation;
                auto write = entry->write;
                auto completion = entry->completion;

                write(next_value);
                if (state->closing) return;
                target_invalidation.invalidate(invalidation);
                if (state->closing) return;

                if (!completed || !find_entry(state, id)) continue;
                (void)erase_entry(state, id);
                if (completion) completion();
                if (state->closing) return;
            } catch (...) {
                const auto failure = std::current_exception();
                recover_after_step_failure(state, id);
                std::rethrow_exception(failure);
            }
        }

        if (!state->entries.empty()) {
            try {
                if (!ensure_wake(state)) state->entries.clear();
            } catch (...) {
                state->entries.clear();
                cancel_wake_noexcept(state);
                throw;
            }
        }
    }

    static void shutdown(const std::shared_ptr<State>& state) noexcept {
        if (!state || state->closing) return;
        state->closing = true;
        cancel_wake_noexcept(state);
        state->entries.clear();
    }

    std::shared_ptr<State> state_;
};

} // namespace ui
