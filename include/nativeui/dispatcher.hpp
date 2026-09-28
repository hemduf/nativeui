#pragma once

/// \file
/// Owner-scoped worker-to-UI dispatch and timer scheduling.
///
/// Dispatcher is a weak, copyable handle to one concrete NativeUI event-loop
/// owner. Queue/timer mutation is synchronized for ordinary worker-thread use,
/// while accepted callbacks execute later on that owner's UI/main thread.
/// Posting and timer operations may allocate and lock; this API is never an
/// audio-real-time transport.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace ui {

class AnimationContext;

namespace detail {
struct DispatcherOwnerToken;
struct DispatcherState;
class DispatcherOwner;
struct DispatcherTestAccess;
} // namespace detail

/// Public dispatcher interval/delay unit, expressed in seconds.
///
/// Values are converted to the owner's steady-clock duration when scheduled.
/// Timer APIs reject non-finite, negative or unrepresentable durations;
/// repeating timers additionally reject zero (including values that round to a
/// zero native duration).
using DispatcherDuration = std::chrono::duration<double>;

/// Maximum accepted queued callbacks owned by one Dispatcher owner.
inline constexpr std::size_t kDispatcherMaxPendingTasks = 65'536;
/// Maximum active one-shot plus repeating timers owned by one Dispatcher owner.
inline constexpr std::size_t kDispatcherMaxActiveTimers = 8'192;
/// Maximum callbacks begun from the fixed work snapshot of one UI checkpoint.
inline constexpr std::size_t kDispatcherMaxTasksPerCheckpoint = 1'024;

/// Opaque timer identity scoped to exactly one logical Dispatcher owner.
///
/// A handle never keeps the Dispatcher queue or its native wake backend alive.
/// The small immutable owner token is retained only to make stale/cross-owner
/// cancellation deterministic even if allocator addresses are later reused.
class TimerHandle final {
public:
    /// Construct an invalid handle.
    TimerHandle() = default;

    /// True when this value contains an owner/id timer identity.
    ///
    /// This does not prove the timer is still active: it may already have fired,
    /// been cancelled or lost its Dispatcher owner.
    [[nodiscard]] bool valid() const noexcept { return owner_ && id_ != 0; }

    /// Convenience spelling for valid().
    explicit operator bool() const noexcept { return valid(); }

    /// Compare immutable owner identity plus timer id.
    ///
    /// Equality does not imply that either timer is still active.
    friend bool operator==(const TimerHandle&, const TimerHandle&) noexcept = default;

private:
    TimerHandle(std::shared_ptr<const detail::DispatcherOwnerToken> owner,
                std::uint64_t id) noexcept
        : owner_(std::move(owner)), id_(id) {}

    std::shared_ptr<const detail::DispatcherOwnerToken> owner_;
    std::uint64_t id_{};

    friend class Dispatcher;
    friend struct detail::DispatcherState;
};

/// Canonical invalid timer value used when no live schedule was accepted.
inline const TimerHandle kInvalidTimerHandle{};

/// Weak, copyable handle to one logical UI/window/view dispatcher.
///
/// `post()` is thread-safe but intentionally not real-time safe: it may allocate
/// and synchronize. Timer operations have the same non-RT contract. Accepted
/// callbacks execute only when the owning UI/event-loop checkpoint is pumped.
class Dispatcher final {
public:
    /// Owned callback type transferred into the queue/timer on acceptance.
    ///
    /// Captures can outlive the posting call and remain owned until execution,
    /// cancellation or owner shutdown. Reference captures remain the caller's
    /// lifetime responsibility.
    using Callback = std::function<void()>;

    /// Construct an invalid dispatcher with no owner.
    Dispatcher() = default;

    /// Enqueue one callback for a later checkpoint on this owner's UI thread.
    ///
    /// Returns true only after ownership of a non-empty callback enters the
    /// bounded FIFO queue. Returns false for an invalid/closing owner, an empty
    /// callback, a full queue or exhausted internal identity space. Allocation
    /// failure may throw; a throwing call has not accepted the callback.
    ///
    /// Accepted work is never invoked inline. Reentrant post() calls made by a
    /// running callback are outside that checkpoint's fixed snapshot and wait
    /// for a later checkpoint. A throwing callback consumes that invocation;
    /// later accepted callbacks remain queued.
    ///
    /// Thread-safe for ordinary worker/UI callers, but not real-time safe.
    [[nodiscard]] bool post(Callback callback) const;

    /// Schedule one callback after at least \p delay seconds of dispatcher time.
    ///
    /// \p delay must be finite, non-negative, representable by the owner's
    /// steady clock and safe to add to its current time. Zero is valid but still
    /// defers execution to a dispatcher checkpoint. Invalid input/owner, an
    /// empty callback or timer-capacity exhaustion returns an invalid handle.
    /// Allocation may throw before a timer is accepted.
    [[nodiscard]] TimerHandle schedule_after(DispatcherDuration delay,
                                             Callback callback) const;

    /// Schedule a fixed-delay repeating callback.
    ///
    /// \p interval is in seconds and must be finite, strictly positive,
    /// representable by the owner's steady clock, and remain non-zero after
    /// native-duration conversion. Missed periods are not replayed as catch-up
    /// bursts: the next deadline is current checkpoint time plus \p interval.
    ///
    /// The same owned std::function object is retained across repetitions, so
    /// mutable callback state persists. Invalid input/owner, an empty callback
    /// or timer-capacity exhaustion returns an invalid handle; allocation may
    /// throw before acceptance.
    [[nodiscard]] TimerHandle schedule_every(DispatcherDuration interval,
                                             Callback callback) const;

    /// Remove an active timer schedule owned by this Dispatcher.
    ///
    /// Returns false for an invalid/stale handle, a handle from another owner,
    /// an already-fired/cancelled timer, or an invalid/closing Dispatcher.
    /// Successful cancellation removes future scheduling only; it cannot retract
    /// a firing already transferred to the task queue or already executing.
    ///
    /// Thread-safe, potentially synchronizing, and not real-time safe.
    [[nodiscard]] bool cancel(const TimerHandle& handle) const;

    /// Whether the weak owner still exists and is accepting work.
    ///
    /// A true result is only a lifetime snapshot; a later operation can still
    /// be rejected because shutdown or bounded capacity changed concurrently.
    [[nodiscard]] bool valid() const noexcept;

private:
    explicit Dispatcher(const std::shared_ptr<detail::DispatcherState>& state) noexcept
        : state_(state) {}

    /// Internal T065 clock access used by T040. Keeping this private prevents a
    /// second public clock API while allowing animation elapsed-time math to use
    /// the exact injected steady/manual clock that schedules Dispatcher timers.
    [[nodiscard]] std::chrono::steady_clock::time_point current_time() const noexcept;

    std::weak_ptr<detail::DispatcherState> state_;

    friend class AnimationContext;
    friend class detail::DispatcherOwner;
    friend struct detail::DispatcherTestAccess;
};

/// Optional platform capability exposing the T065 dispatcher that owns a
/// concrete native UI/event-loop instance. PlatformServices stays focused on
/// drawing/input services; retained policies such as Tooltip discover timing
/// only when the concrete platform object supplies this capability.
class DispatcherProvider {
public:
    /// Polymorphic discovery seam; this interface does not own the queue.
    virtual ~DispatcherProvider() = default;

    /// Return the weak Dispatcher handle associated with this concrete owner.
    ///
    /// The returned value owns no event loop/backend and may become invalid when
    /// that owner shuts down. Implementations must not substitute a process-wide
    /// or thread-local "current UI" dispatcher.
    [[nodiscard]] virtual Dispatcher dispatcher() const noexcept = 0;
};

} // namespace ui
