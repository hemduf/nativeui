#pragma once

// Explicit native Pugl operations and per-instance capture adapters.
#include <nativeui/nativeui.hpp>
#include <pugl/pugl.h>

#if defined(__linux__)
#  include <X11/Xlib.h>
#  undef None
#elif defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif
namespace ui::detail {

#if defined(__linux__)
struct X11ViewBridge final {
    PuglView* view{};
    void* user_handle{};
    PuglEventFunc callback{};
    bool focus_known{};
    bool focused{};
};

class X11PointerCapturePlatformServices : public PlatformServices {
public:
    void bind_view(PuglView* view, void* user_handle) noexcept {
        bridge_ = X11ViewBridge{view, user_handle, nullptr, false, false};
        display_ = view
            ? static_cast<Display*>(puglGetNativeWorld(puglGetWorld(view)))
            : nullptr;
        ::puglSetHandle(view, &bridge_);
    }

    [[nodiscard]] X11ViewBridge* bridge_for(PuglView* view) noexcept {
        return bridge_.view == view ? &bridge_ : nullptr;
    }

    void begin_pointer_capture() noexcept override {
        pointer_capture_active_ = display_ && bridge_.view;
    }

    void end_pointer_capture() noexcept override {
        if (!pointer_capture_active_) return;
        pointer_capture_active_ = false;
        if (!display_) return;
        XUngrabPointer(display_, CurrentTime);
        XSync(display_, False);
    }

private:
    X11ViewBridge bridge_{};
    Display* display_{};
    bool pointer_capture_active_{};
};

[[nodiscard]] inline X11ViewBridge* x11_bridge(PuglView* view) noexcept {
    return view ? static_cast<X11ViewBridge*>(::puglGetHandle(view)) : nullptr;
}

inline PuglStatus dispatch_x11_focus_transition(
    PuglView* view, X11ViewBridge& bridge, bool focused) noexcept {
    if (bridge.focus_known && bridge.focused == focused) return PUGL_SUCCESS;
    if (!bridge.focus_known && !focused) {
        bridge.focus_known = true;
        bridge.focused = false;
        return PUGL_SUCCESS;
    }

    bridge.focus_known = true;
    bridge.focused = focused;
    if (!bridge.callback) return PUGL_SUCCESS;

    PuglEvent focus_event{};
    focus_event.focus.type = focused ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT;
    focus_event.focus.mode = PUGL_CROSSING_NORMAL;
    return bridge.callback(view, &focus_event);
}

inline PuglStatus x11_event_proxy(PuglView* view, const PuglEvent* event) noexcept {
    auto* bridge = x11_bridge(view);
    if (!bridge || !bridge->callback) return PUGL_SUCCESS;

    if (event && (event->type == PUGL_FOCUS_IN || event->type == PUGL_FOCUS_OUT)) {
        const bool focused = event->type == PUGL_FOCUS_IN;
        if (bridge->focus_known && bridge->focused == focused) return PUGL_SUCCESS;
        bridge->focus_known = true;
        bridge->focused = focused;
        return bridge->callback(view, event);
    }

    const bool focused = puglHasFocus(view);
    if (const auto status = dispatch_x11_focus_transition(view, *bridge, focused)) {
        return status;
    }
    return bridge->callback(view, event);
}

inline void tracked_pugl_set_handle(
    PuglView* view,
    void* user_handle,
    X11PointerCapturePlatformServices& services) noexcept {
    services.bind_view(view, user_handle);
}

inline void* tracked_pugl_get_handle(PuglView* view) noexcept {
    auto* bridge = x11_bridge(view);
    return bridge ? bridge->user_handle : nullptr;
}

inline PuglStatus tracked_pugl_set_event_func(PuglView* view, PuglEventFunc callback) noexcept {
    auto* bridge = x11_bridge(view);
    if (!bridge) return PUGL_BAD_PARAMETER;
    bridge->callback = callback;
    const auto status = ::puglSetEventFunc(view, &x11_event_proxy);
    if (status != PUGL_SUCCESS) bridge->callback = nullptr;
    return status;
}

inline void tracked_pugl_free_view(PuglView* view) noexcept {
    auto* bridge = x11_bridge(view);
    ::puglFreeView(view);
    if (bridge) {
        bridge->view = nullptr;
        bridge->user_handle = nullptr;
        bridge->callback = nullptr;
        bridge->focus_known = false;
        bridge->focused = false;
    }
}
#endif

#if defined(_WIN32)
class WinPointerCapturePlatformServices : public PlatformServices {
public:
    void begin_pointer_capture() noexcept override {
        captured_window_ = GetCapture();
    }

    void end_pointer_capture() noexcept override {
        const HWND captured = captured_window_;
        captured_window_ = nullptr;
        if (captured && GetCapture() == captured) {
            (void)ReleaseCapture();
        }
    }

private:
    HWND captured_window_{};
};
#endif

#if defined(__linux__)
using ViewPlatformServices = X11PointerCapturePlatformServices;
using WindowPlatformServices = X11PointerCapturePlatformServices;
#elif defined(_WIN32)
using ViewPlatformServices = ::ui::PlatformServices;
using WindowPlatformServices = WinPointerCapturePlatformServices;
#else
using ViewPlatformServices = ::ui::PlatformServices;
using WindowPlatformServices = ::ui::PlatformServices;
#endif

inline void set_view_handle(PuglView* view, void* handle, ViewPlatformServices& services) noexcept {
#if defined(__linux__)
    tracked_pugl_set_handle(view, handle, services);
#else
    (void)services;
    ::puglSetHandle(view, handle);
#endif
}

[[nodiscard]] inline void* get_view_handle(PuglView* view) noexcept {
#if defined(__linux__)
    return tracked_pugl_get_handle(view);
#else
    return ::puglGetHandle(view);
#endif
}

[[nodiscard]] inline PuglStatus set_view_event_func(PuglView* view,
                                                    PuglEventFunc callback) noexcept {
#if defined(__linux__)
    return tracked_pugl_set_event_func(view, callback);
#else
    return ::puglSetEventFunc(view, callback);
#endif
}

inline void free_view(PuglView* view) noexcept {
#if defined(__linux__)
    tracked_pugl_free_view(view);
#else
    ::puglFreeView(view);
#endif
}

[[nodiscard]] inline PuglStatus show_pugl_view(PuglView* view, PuglShowCommand command) {
    const auto status = ::puglShow(view, command);
    // Raising a visible window may be denied by the window manager.
    if (command == PUGL_SHOW_RAISE && status == PUGL_FAILURE) return PUGL_SUCCESS;
    return status;
}

} // namespace ui::detail
