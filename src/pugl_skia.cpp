#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>

#include "detail/native_screen_origin.hpp"
#include "detail/semantic_native_bounds.hpp"
#include "detail/pugl_button_translation.hpp"
#include "detail/pugl_pointer_translation.hpp"
#include "detail/scoped_borrow_state.hpp"
#include "detail/window_control_state.hpp"

#if defined(__APPLE__)
#  include <CoreFoundation/CFRunLoop.h>
#elif defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <poll.h>
#  undef None
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>

#include "detail/application_platform_state.hpp"
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
#  include "detail/platform_test_access.hpp"
#endif
#if defined(__APPLE__)
#  include "detail/macos_desktop_services.hpp"
#elif defined(_WIN32)
#  include "detail/windows_desktop_services.hpp"
#elif defined(__linux__)
#  include "detail/linux_desktop_services.hpp"
#endif
#include "detail/native_accessibility_binding.hpp"
#include "detail/native_accessibility_bridge.h"
#include "detail/native_ime_bridge.h"
#include "detail/pugl_skia_setup.inc"
#include "detail/pugl_skia_show_policy.inc"

} // namespace
namespace detail {

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

[[nodiscard]] X11ViewBridge* x11_bridge(PuglView* view) noexcept {
    return view ? static_cast<X11ViewBridge*>(::puglGetHandle(view)) : nullptr;
}

PuglStatus dispatch_x11_focus_transition(
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

PuglStatus x11_event_proxy(PuglView* view, const PuglEvent* event) noexcept {
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

void tracked_pugl_set_handle(
    PuglView* view,
    void* user_handle,
    X11PointerCapturePlatformServices& services) noexcept {
    services.bind_view(view, user_handle);
}

void* tracked_pugl_get_handle(PuglView* view) noexcept {
    auto* bridge = x11_bridge(view);
    return bridge ? bridge->user_handle : nullptr;
}

PuglStatus tracked_pugl_set_event_func(PuglView* view, PuglEventFunc callback) noexcept {
    auto* bridge = x11_bridge(view);
    if (!bridge) return PUGL_BAD_PARAMETER;
    bridge->callback = callback;
    const auto status = ::puglSetEventFunc(view, &x11_event_proxy);
    if (status != PUGL_SUCCESS) bridge->callback = nullptr;
    return status;
}

void tracked_pugl_free_view(PuglView* view) noexcept {
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

#  define PlatformServices ::ui::detail::X11PointerCapturePlatformServices
#  define puglSetHandle(view, handle) \
    ::ui::detail::tracked_pugl_set_handle((view), (handle), services_)
#  define puglGetHandle(view) ::ui::detail::tracked_pugl_get_handle((view))
#  define puglSetEventFunc(view, callback) \
    ::ui::detail::tracked_pugl_set_event_func((view), (callback))
#  define puglFreeView(view) ::ui::detail::tracked_pugl_free_view((view))
#endif

#include "detail/pugl_skia_view_a.inc"
#include "detail/pugl_skia_view_b.inc"

#if defined(__linux__)
#  undef puglFreeView
#  undef puglSetEventFunc
#  undef puglGetHandle
#  undef puglSetHandle
#endif

using ViewCore = detail::ViewCore;

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

#  define PlatformServices WinPointerCapturePlatformServices
#endif

#include "detail/pugl_skia_windows.inc"
#include "detail/pugl_skia_desktop_services.inc"

#if defined(__linux__) || defined(_WIN32)
#  undef PlatformServices
#endif

#if defined(__linux__)
namespace detail {

class X11FaultProbePlatformServices final : public X11PointerCapturePlatformServices {
public:
    explicit X11FaultProbePlatformServices(PlatformServices& services) noexcept
        : services_(services) {}

    void set_clipboard_text(std::string_view text) override {
        services_.set_clipboard_text(text);
    }

    void request_clipboard_text() override {
        services_.request_clipboard_text();
    }

private:
    PlatformServices& services_;
};

NativeViewConstructionFaultResult exercise_native_view_construction_fault(
    UI& ui,
    PlatformServices& services,
    NativeParentHandle parent,
    Size size,
    NativeViewConstructionFaultStage stage) noexcept {
    X11FaultProbePlatformServices probe_services{services};
    return exercise_native_view_construction_fault(
        ui, probe_services, parent, size, stage);
}

} // namespace detail
#endif

} // namespace ui

#include "detail/pugl_skia_t043.inc"
