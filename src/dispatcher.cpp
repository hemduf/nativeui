#include <nativeui/detail/dispatcher_owner.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace ui::detail {

struct DispatcherOwnerToken final {};

struct TimerEntry final {
    std::uint64_t id{};
    std::uint64_t sequence{};
    DispatcherTimePoint due{};
    std::chrono::steady_clock::duration interval{};
    bool repeating{};
    Dispatcher::Callback callback;
};

namespace {

class SteadyDispatcherClock final : public DispatcherClock {
public:
    [[nodiscard]] DispatcherTimePoint now() const noexcept override {
        return std::chrono::steady_clock::now();
    }
};

[[nodiscard]] std::optional<std::chrono::steady_clock::duration>
checked_duration(DispatcherDuration value, bool zero_allowed) noexcept {
    const double seconds = value.count();
    if (!std::isfinite(seconds) || seconds < 0.0 || (!zero_allowed && seconds == 0.0)) {
        return std::nullopt;
    }

    using NativeDuration = std::chrono::steady_clock::duration;
    const double maximum = std::chrono::duration<double>(NativeDuration::max()).count();
    if (seconds > maximum) return std::nullopt;

    const auto converted = std::chrono::duration_cast<NativeDuration>(value);
    if (converted < NativeDuration::zero() || (!zero_allowed && converted == NativeDuration::zero())) {
        return std::nullopt;
    }
    return converted;
}

[[nodiscard]] bool can_add(DispatcherTimePoint now,
                           std::chrono::steady_clock::duration delta) noexcept {
    return delta <= DispatcherTimePoint::max().time_since_epoch() - now.time_since_epoch();
}

} // namespace

struct DispatcherState final {
    explicit DispatcherState(std::weak_ptr<DispatcherWakeBackend> backend,
                             std::shared_ptr<DispatcherClock> dispatcher_clock)
        : wake_backend(std::move(backend)),
          clock(dispatcher_clock ? std::move(dispatcher_clock)
                                 : std::make_shared<SteadyDispatcherClock>()),
          token(std::make_shared<DispatcherOwnerToken>()) {}

    [[nodiscard]] TimerHandle make_timer_handle(std::uint64_t id) const noexcept {
        return TimerHandle{token, id};
    }

    mutable std::mutex mutex;
    std::deque<Dispatcher::Callback> tasks;
    std::vector<TimerEntry> timers;
    std::weak_ptr<DispatcherWakeBackend> wake_backend;
    std::shared_ptr<DispatcherClock> clock;
    std::shared_ptr<const DispatcherOwnerToken> token;
    std::uint64_t next_timer_id{1};
    std::uint64_t next_timer_sequence{1};
    bool closing{};
    bool wake_pending{};
};

void request_dispatcher_wake(const std::shared_ptr<DispatcherState>& state) noexcept {
    std::shared_ptr<DispatcherWakeBackend> backend;
    {
        std::lock_guard lock{state->mutex};
        if (state->closing || !state->wake_pending) return;
        backend = state->wake_backend.lock();
    }
    if (backend) backend->request_wake();
}

[[nodiscard]] TimerHandle schedule_dispatcher_timer(
    const std::weak_ptr<DispatcherState>& weak_state,
    DispatcherDuration duration,
    bool repeating,
    Dispatcher::Callback callback) {
    if (!callback) return {};
    const auto converted = checked_duration(duration, !repeating);
    if (!converted) return {};

    const auto state = weak_state.lock();
    if (!state) return {};
    const auto now = state->clock->now();
    if (!can_add(now, *converted)) return {};

    TimerHandle handle;
    bool should_wake = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->closing || state->timers.size() >= kDispatcherMaxActiveTimers) return {};
        if (state->next_timer_id == 0 || state->next_timer_sequence == 0) return {};

        const auto id = state->next_timer_id++;
        const auto sequence = state->next_timer_sequence++;
        handle = state->make_timer_handle(id);
        state->timers.push_back(TimerEntry{
            id,
            sequence,
            now + *converted,
            repeating ? *converted : std::chrono::steady_clock::duration::zero(),
            repeating,
            std::move(callback),
        });

        if (!state->wake_pending) {
            state->wake_pending = true;
            should_wake = true;
        }
    }
    if (should_wake) request_dispatcher_wake(state);
    return handle;
}

void ManualDispatcherClock::advance(DispatcherDuration delta) noexcept {
    const auto converted = checked_duration(delta, true);
    if (!converted || !can_add(now_, *converted)) return;
    now_ += *converted;
}

DispatcherOwner::DispatcherOwner(std::shared_ptr<DispatcherWakeBackend> wake_backend,
                                 std::shared_ptr<DispatcherClock> clock)
    : state_(std::make_shared<DispatcherState>(wake_backend, std::move(clock))) {}

DispatcherOwner::~DispatcherOwner() {
    shutdown();
}

Dispatcher DispatcherOwner::dispatcher() const noexcept {
    return Dispatcher{state_};
}

std::size_t DispatcherOwner::checkpoint() {
    // Keep the queue state alive independently from `this`: a user callback is
    // allowed to destroy its logical native owner while this method is active.
    const auto state = state_;
    if (!state) return 0;

    const auto now = state->clock->now();
    std::vector<Dispatcher::Callback> snapshot;
    snapshot.reserve(kDispatcherMaxTasksPerCheckpoint);
    bool should_wake = false;

    {
        std::lock_guard lock{state->mutex};
        if (state->closing) return 0;
        state->wake_pending = false;

        struct DueTimer final {
            DispatcherTimePoint due;
            std::uint64_t sequence;
            std::uint64_t id;
        };
        std::vector<DueTimer> due;
        due.reserve(state->timers.size());
        for (const auto& timer : state->timers) {
            if (timer.due <= now) due.push_back({timer.due, timer.sequence, timer.id});
        }
        std::sort(due.begin(), due.end(), [](const DueTimer& left, const DueTimer& right) {
            if (left.due != right.due) return left.due < right.due;
            return left.sequence < right.sequence;
        });

        bool timer_blocked_by_full_queue = false;
        for (const auto& candidate : due) {
            if (state->tasks.size() >= kDispatcherMaxPendingTasks) {
                timer_blocked_by_full_queue = true;
                break;
            }

            const auto it = std::find_if(state->timers.begin(), state->timers.end(),
                                         [&](const TimerEntry& timer) {
                                             return timer.id == candidate.id;
                                         });
            if (it == state->timers.end() || it->due > now) continue;

            state->tasks.push_back(it->callback);
            if (it->repeating) {
                if (!can_add(now, it->interval)) {
                    state->timers.erase(it);
                } else {
                    it->due = now + it->interval;
                }
            } else {
                // One-shot becomes inactive immediately after successful queue
                // insertion and before its callback can execute.
                state->timers.erase(it);
            }
        }

        const std::size_t count =
            std::min(state->tasks.size(), kDispatcherMaxTasksPerCheckpoint);
        for (std::size_t i = 0; i < count; ++i) {
            snapshot.push_back(std::move(state->tasks.front()));
            state->tasks.pop_front();
        }

        if (!state->tasks.empty() || timer_blocked_by_full_queue) {
            state->wake_pending = true;
            should_wake = true;
        }
    }

    if (should_wake) request_dispatcher_wake(state);

    std::size_t executed = 0;
    for (auto& callback : snapshot) {
        {
            std::lock_guard lock{state->mutex};
            if (state->closing) break;
        }
        callback();
        ++executed;
    }
    return executed;
}

void DispatcherOwner::shutdown() noexcept {
    const auto state = state_;
    if (!state) return;
    {
        std::lock_guard lock{state->mutex};
        state->closing = true;
        state->tasks.clear();
        state->timers.clear();
        state->wake_pending = false;
        state->wake_backend.reset();
    }
    state_.reset();
}

std::size_t DispatcherOwner::pending_task_count() const noexcept {
    const auto state = state_;
    if (!state) return 0;
    std::lock_guard lock{state->mutex};
    return state->tasks.size();
}

std::size_t DispatcherOwner::active_timer_count() const noexcept {
    const auto state = state_;
    if (!state) return 0;
    std::lock_guard lock{state->mutex};
    return state->timers.size();
}

std::optional<DispatcherDuration> DispatcherOwner::next_delay() const noexcept {
    const auto state = state_;
    if (!state) return std::nullopt;
    const auto now = state->clock->now();

    std::lock_guard lock{state->mutex};
    if (state->closing) return std::nullopt;
    if (!state->tasks.empty()) return DispatcherDuration::zero();
    if (state->timers.empty()) return std::nullopt;

    const auto it = std::min_element(state->timers.begin(), state->timers.end(),
                                     [](const TimerEntry& left, const TimerEntry& right) {
                                         if (left.due != right.due) return left.due < right.due;
                                         return left.sequence < right.sequence;
                                     });
    if (it->due <= now) return DispatcherDuration::zero();
    return std::chrono::duration_cast<DispatcherDuration>(it->due - now);
}

} // namespace ui::detail

namespace ui {

bool Dispatcher::post(Callback callback) const {
    if (!callback) return false;
    const auto state = state_.lock();
    if (!state) return false;

    bool should_wake = false;
    {
        std::lock_guard lock{state->mutex};
        if (state->closing || state->tasks.size() >= kDispatcherMaxPendingTasks) return false;
        const bool was_empty = state->tasks.empty();
        state->tasks.push_back(std::move(callback));
        if (was_empty && !state->wake_pending) {
            state->wake_pending = true;
            should_wake = true;
        }
    }
    if (should_wake) detail::request_dispatcher_wake(state);
    return true;
}

TimerHandle Dispatcher::schedule_after(DispatcherDuration delay, Callback callback) const {
    return detail::schedule_dispatcher_timer(state_, delay, false, std::move(callback));
}

TimerHandle Dispatcher::schedule_every(DispatcherDuration interval, Callback callback) const {
    return detail::schedule_dispatcher_timer(state_, interval, true, std::move(callback));
}

bool Dispatcher::cancel(const TimerHandle& handle) const {
    if (!handle.valid()) return false;
    const auto state = state_.lock();
    if (!state || handle.owner_.get() != state->token.get()) return false;

    std::lock_guard lock{state->mutex};
    if (state->closing) return false;
    const auto it = std::find_if(state->timers.begin(), state->timers.end(),
                                 [&](const detail::TimerEntry& timer) {
                                     return timer.id == handle.id_;
                                 });
    if (it == state->timers.end()) return false;
    state->timers.erase(it);
    return true;
}

bool Dispatcher::valid() const noexcept {
    const auto state = state_.lock();
    if (!state) return false;
    std::lock_guard lock{state->mutex};
    return !state->closing;
}

} // namespace ui
