#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace ui {

namespace detail {
struct DispatcherOwnerToken;
struct DispatcherState;
class DispatcherOwner;
} // namespace detail

using DispatcherDuration = std::chrono::duration<double>;

inline constexpr std::size_t kDispatcherMaxPendingTasks = 65'536;
inline constexpr std::size_t kDispatcherMaxActiveTimers = 8'192;
inline constexpr std::size_t kDispatcherMaxTasksPerCheckpoint = 1'024;

/// Opaque timer identity scoped to exactly one logical Dispatcher owner.
///
/// A handle never keeps the Dispatcher queue or its native wake backend alive.
/// The small immutable owner token is retained only to make stale/cross-owner
/// cancellation deterministic even if allocator addresses are later reused.
class TimerHandle final {
public:
    TimerHandle() = default;

    [[nodiscard]] bool valid() const noexcept { return owner_ && id_ != 0; }
    explicit operator bool() const noexcept { return valid(); }

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

inline const TimerHandle kInvalidTimerHandle{};

/// Weak, copyable handle to one logical UI/window/view dispatcher.
///
/// `post()` is thread-safe but intentionally not real-time safe: it may allocate
/// and synchronize. Timer operations have the same non-RT contract. Accepted
/// callbacks execute only when the owning UI/event-loop checkpoint is pumped.
class Dispatcher final {
public:
    using Callback = std::function<void()>;

    Dispatcher() = default;

    [[nodiscard]] bool post(Callback callback) const;
    [[nodiscard]] TimerHandle schedule_after(DispatcherDuration delay,
                                             Callback callback) const;
    [[nodiscard]] TimerHandle schedule_every(DispatcherDuration interval,
                                             Callback callback) const;
    [[nodiscard]] bool cancel(const TimerHandle& handle) const;
    [[nodiscard]] bool valid() const noexcept;

private:
    explicit Dispatcher(const std::shared_ptr<detail::DispatcherState>& state) noexcept
        : state_(state) {}

    std::weak_ptr<detail::DispatcherState> state_;

    friend class detail::DispatcherOwner;
};

/// Optional platform capability exposing the T065 dispatcher that owns a
/// concrete native UI/event-loop instance. PlatformServices stays focused on
/// drawing/input services; retained policies such as Tooltip discover timing
/// only when the concrete platform object supplies this capability.
class DispatcherProvider {
public:
    virtual ~DispatcherProvider() = default;
    [[nodiscard]] virtual Dispatcher dispatcher() const noexcept = 0;
};

} // namespace ui
