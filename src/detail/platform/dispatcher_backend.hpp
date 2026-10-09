#pragma once

// Pugl event-loop wake backend and dispatcher checkpoints.
#include <nativeui/nativeui.hpp>
#include <pugl/gl.h>
#include <pugl/pugl.h>
#include <nativeui/detail/dispatcher_owner.hpp>
#include "../application_platform_state.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#if defined(__APPLE__)
#  include <CoreFoundation/CFRunLoop.h>
#elif defined(_WIN32)
#  include <windows.h>
#elif defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <poll.h>
#  undef None
#endif

namespace ui {

namespace platform_detail {

class PuglDispatcherWakeBackend final : public detail::DispatcherWakeBackend {
public:
    explicit PuglDispatcherWakeBackend(PuglWorld* world) noexcept
        : world_(world) {}

    ~PuglDispatcherWakeBackend() override = default;

    [[nodiscard]] bool ensure_view();

    [[nodiscard]] std::string_view last_error() const noexcept;

    void request_wake() noexcept override;

    [[nodiscard]] PuglStatus update_world(double timeout_seconds) noexcept;

    void begin_shutdown() noexcept;

    void destroy_view_on_ui_thread() noexcept;

private:
    mutable std::mutex mutex_;
    PuglWorld* world_{};
    PuglView* view_{};
#if defined(__APPLE__)
    CFRunLoopRef run_loop_{};
    CFRunLoopSourceRef wake_source_{};
#elif defined(_WIN32)
    HWND native_window_{};
#elif defined(__linux__)
    Display* display_{};
    Window native_window_{};
    Atom wake_atom_{};
#endif
    bool closing_{};
    std::string last_error_;
};

struct ApplicationWindowRecord final {
    bool open{true};
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
    std::weak_ptr<int> lifetime;
    void* close_control_context{};
    void (*close_control_checkpoint)(void*){};
};

struct WindowControlCheckpoint final {
    std::weak_ptr<int> lifetime;
    void* context{};
    void (*checkpoint)(void*){};
};

[[nodiscard]] double bound_dispatch_timeout(
    double requested,
    const std::vector<std::shared_ptr<detail::DispatcherOwner>>& owners) noexcept;

} // namespace platform_detail

} // namespace ui
