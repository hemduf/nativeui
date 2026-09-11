#include <nativeui/detail/dispatcher_owner.hpp>

#if defined(__APPLE__)
#  include <CoreFoundation/CFRunLoop.h>
#elif defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <poll.h>
#  undef None
#endif

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <mutex>
#include <vector>

#include "detail/native_ime_bridge.h"
#include "detail/pugl_skia_setup.inc"
#include "detail/pugl_skia_show_policy.inc"

} // namespace
namespace detail {

#if defined(__linux__)
// The pinned Pugl X11 backend translates FocusIn/FocusOut but consumes those
// translated events only for XIM focus bookkeeping instead of forwarding them
// to the application callback. Keep NativeUI's retained activation state in
// sync with the authoritative X11 input focus immediately before Pugl drains
// its event queue. This also preserves the first pointer press after a focus
// transition instead of letting Tree::dispatch reject it while inactive.
struct X11FocusRecord final {
    PuglEventFunc callback{};
    bool known{};
    bool focused{};
};

std::unordered_map<PuglView*, X11FocusRecord>& x11_focus_records() {
    static std::unordered_map<PuglView*, X11FocusRecord> records;
    return records;
}

thread_local PuglView* x11_callback_view = nullptr;

class X11CallbackScope final {
public:
    explicit X11CallbackScope(PuglView* view) noexcept
        : previous_(x11_callback_view) {
        x11_callback_view = view;
    }

    X11CallbackScope(const X11CallbackScope&) = delete;
    X11CallbackScope& operator=(const X11CallbackScope&) = delete;

    ~X11CallbackScope() { x11_callback_view = previous_; }

private:
    PuglView* previous_{};
};

PuglView* current_x11_callback_view() noexcept {
    return x11_callback_view;
}

PuglStatus invoke_x11_callback(
    PuglView* view, PuglEventFunc callback, const PuglEvent* event) noexcept {
    if (!callback) return PUGL_SUCCESS;
    X11CallbackScope scope{view};
    return callback(view, event);
}

PuglStatus dispatch_x11_focus_transition(PuglView* view, bool focused) noexcept {
    auto& records = x11_focus_records();
    const auto found = records.find(view);
    if (found == records.end()) return PUGL_SUCCESS;

    auto& record = found->second;
    if (record.known && record.focused == focused) return PUGL_SUCCESS;

    // A newly registered, currently unfocused view is already in NativeUI's
    // inactive state. Learn that baseline without manufacturing a FocusOut
    // callback that the native system never delivered to an active view.
    if (!record.known && !focused) {
        record.known = true;
        record.focused = false;
        return PUGL_SUCCESS;
    }

    const auto callback = record.callback;
    record.known = true;
    record.focused = focused;

    PuglEvent event{};
    event.focus.type = focused ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT;
    event.focus.mode = PUGL_CROSSING_NORMAL;
    return invoke_x11_callback(view, callback, &event);
}

PuglStatus x11_event_proxy(PuglView* view, const PuglEvent* event) noexcept {
    auto& records = x11_focus_records();
    const auto found = records.find(view);
    if (found == records.end() || !found->second.callback) return PUGL_SUCCESS;

    const auto callback = found->second.callback;
    if (event && (event->type == PUGL_FOCUS_IN || event->type == PUGL_FOCUS_OUT)) {
        const bool focused = event->type == PUGL_FOCUS_IN;
        if (found->second.known && found->second.focused == focused) {
            return PUGL_SUCCESS;
        }
        found->second.known = true;
        found->second.focused = focused;
    }
    return invoke_x11_callback(view, callback, event);
}

PuglStatus tracked_pugl_set_event_func(PuglView* view, PuglEventFunc callback) {
    const auto status = ::puglSetEventFunc(view, &x11_event_proxy);
    if (status == PUGL_SUCCESS) {
        x11_focus_records()[view] = X11FocusRecord{callback, false, false};
    }
    return status;
}

void tracked_pugl_free_view(PuglView* view) {
    // Keep the proxy record alive while Pugl performs its implicit unrealize:
    // PUGL_UNREALIZE must still reach the NativeUI callback on cleanup paths
    // that call puglFreeView() without an explicit preceding unrealize.
    ::puglFreeView(view);
    x11_focus_records().erase(view);
}

bool x11_focus_sync_needed(PuglWorld* world) {
    for (const auto& [view, record] : x11_focus_records()) {
        if (puglGetWorld(view) == world && !record.known) return true;
    }
    return false;
}

PuglStatus sync_x11_focus(PuglWorld* world) noexcept {
    auto* display = static_cast<Display*>(puglGetNativeWorld(world));
    if (!display) return PUGL_FAILURE;

    Window focused_window = 0;
    int revert_to = 0;
    XGetInputFocus(display, &focused_window, &revert_to);

    std::vector<PuglView*> views;
    views.reserve(x11_focus_records().size());
    for (const auto& [view, record] : x11_focus_records()) {
        (void)record;
        if (puglGetWorld(view) == world) views.push_back(view);
    }

    for (auto* view : views) {
        if (x11_focus_records().find(view) == x11_focus_records().end()) continue;
        const auto native_view = static_cast<Window>(puglGetNativeView(view));
        const bool focused = native_view != 0 && native_view == focused_window;
        if (const auto status = dispatch_x11_focus_transition(view, focused)) return status;
    }
    return PUGL_SUCCESS;
}

PuglStatus tracked_pugl_update(PuglWorld* world, double timeout_seconds) {
    auto* display = static_cast<Display*>(puglGetNativeWorld(world));
    if (!display) return PUGL_FAILURE;

    bool have_events = XPending(display) > 0;
    if (!have_events && timeout_seconds != 0.0) {
        const int connection = ConnectionNumber(display);
        if (connection < 0) return PUGL_FAILURE;

        int wait_milliseconds = -1;
        if (timeout_seconds > 0.0) {
            const double milliseconds = std::ceil(timeout_seconds * 1000.0);
            wait_milliseconds = static_cast<int>(std::clamp(
                milliseconds, 1.0, static_cast<double>(INT_MAX)));
        }

        pollfd descriptor{connection, POLLIN, 0};
        int result = 0;
        do {
            result = ::poll(&descriptor, 1, wait_milliseconds);
        } while (result < 0 && errno == EINTR);
        if (result < 0) return PUGL_FAILURE;
        have_events = XPending(display) > 0;
    }

    if (have_events || x11_focus_sync_needed(world)) {
        if (const auto status = sync_x11_focus(world)) return status;
    }

    return ::puglUpdate(world, 0.0);
}

#  define puglSetEventFunc ::ui::detail::tracked_pugl_set_event_func
#  define puglFreeView ::ui::detail::tracked_pugl_free_view
#  define puglUpdate ::ui::detail::tracked_pugl_update
#endif

#include "detail/pugl_skia_view_a.inc"
#include "detail/pugl_skia_view_b.inc"

using ViewCore = detail::ViewCore;

#if defined(__linux__)
// X11 creates an implicit active pointer grab for a ButtonPress. A normal
// ButtonRelease ends it automatically, but retained cancellation can happen
// first (focus loss, destruction, modal/lifecycle cancellation). Record only
// the native view whose callback established this retained owner, then release
// that same client connection when the retained owner ends.
class X11PointerCapturePlatformServices : public PlatformServices {
public:
    void begin_pointer_capture() noexcept override {
        auto* view = current_x11_callback_view();
        if (!view) return;

        auto* world = puglGetWorld(view);
        auto* display = world ? static_cast<Display*>(puglGetNativeWorld(world)) : nullptr;
        const auto window = static_cast<Window>(puglGetNativeView(view));
        if (!display || !window) return;

        captured_display_ = display;
        captured_window_ = window;
    }

    void end_pointer_capture() noexcept override {
        auto* display = captured_display_;
        const auto window = captured_window_;
        captured_display_ = nullptr;
        captured_window_ = 0;
        if (!display || !window) return;

        // XUngrabPointer only affects an active grab owned by this X client.
        // Each PlatformServices instance records its own view at the retained
        // none->owner transition, so sibling views cannot clear each other's
        // retained bookkeeping; repeated release is an X11 no-op.
        XUngrabPointer(display, CurrentTime);
        XFlush(display);
    }

private:
    Display* captured_display_{};
    Window captured_window_{};
};

#  define PlatformServices X11PointerCapturePlatformServices
#elif defined(_WIN32)
// Pugl already acquires HWND capture before dispatching a button press and
// releases it on the matching button release. T044 needs the retained tree's
// cancellation path (focus loss/deactivation/destruction) to mirror that
// ownership transition too. Track only the HWND observed when this UI instance
// begins retained capture so one view can never release another view's grab.
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

#if defined(__linux__) || defined(_WIN32)
#  undef PlatformServices
#endif

#if defined(__linux__)
#  undef puglUpdate
#  undef puglFreeView
#  undef puglSetEventFunc
#endif

} // namespace ui