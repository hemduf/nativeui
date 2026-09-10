#pragma once

#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>

namespace ui::detail {

using DispatcherTimePoint = std::chrono::steady_clock::time_point;

/// Internal clock seam. Native owners use the steady clock; deterministic tests
/// can inject ManualDispatcherClock without sleeping or depending on wall time.
class DispatcherClock {
public:
    virtual ~DispatcherClock() = default;
    [[nodiscard]] virtual DispatcherTimePoint now() const noexcept = 0;
};

class ManualDispatcherClock final : public DispatcherClock {
public:
    [[nodiscard]] DispatcherTimePoint now() const noexcept override { return now_; }
    void advance(DispatcherDuration delta) noexcept;

private:
    DispatcherTimePoint now_{};
};

/// Internal event-loop wake seam shared independently from logical Dispatcher
/// ownership. Implementations must make request_wake() safe for worker threads.
/// Several DispatcherOwner instances may reference one backend.
class DispatcherWakeBackend {
public:
    virtual ~DispatcherWakeBackend() = default;
    virtual void request_wake() noexcept = 0;
};

/// Strong owner of one logical queue/timer namespace.
///
/// The wake backend is referenced weakly so queued work can never dereference a
/// destroyed Application/view backend. Dispatcher handles are weak and become
/// inert as soon as shutdown begins, even if checkpoint() is currently inside a
/// user callback.
class DispatcherOwner final {
public:
    explicit DispatcherOwner(std::shared_ptr<DispatcherWakeBackend> wake_backend = {},
                             std::shared_ptr<DispatcherClock> clock = {});
    ~DispatcherOwner();

    DispatcherOwner(const DispatcherOwner&) = delete;
    DispatcherOwner& operator=(const DispatcherOwner&) = delete;
    DispatcherOwner(DispatcherOwner&&) = delete;
    DispatcherOwner& operator=(DispatcherOwner&&) = delete;

    [[nodiscard]] Dispatcher dispatcher() const noexcept;

    /// Process due timers and execute at most kDispatcherMaxTasksPerCheckpoint
    /// callbacks from one fixed snapshot. Returns callbacks actually begun.
    std::size_t checkpoint();

    /// Reject future work and discard queued tasks/timers. Safe to call more
    /// than once, including from a callback currently executing in checkpoint().
    void shutdown() noexcept;

    [[nodiscard]] std::size_t pending_task_count() const noexcept;
    [[nodiscard]] std::size_t active_timer_count() const noexcept;

    /// Delay until useful work is ready. 0 means queued/due work, nullopt means
    /// no work is scheduled. Native event loops use this to bound their wait.
    [[nodiscard]] std::optional<DispatcherDuration> next_delay() const noexcept;

private:
    std::shared_ptr<DispatcherState> state_;
};

} // namespace ui::detail
