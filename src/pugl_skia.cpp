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
    if (!callback) return PUGL_SUCCESS;

    PuglEvent event{};
    event.focus.type = focused ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT;
    event.focus.mode = PUGL_CROSSING_NORMAL;
    return callback(view, &event);
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
    return callback(view, event);
}

PuglStatus tracked_pugl_set_event_func(PuglView* view, PuglEventFunc callback) {
    const auto status = ::puglSetEventFunc(view, &x11_event_proxy);
    if (status == PUGL_SUCCESS) {
        x11_focus_records()[view] = X11FocusRecord{callback, false, false};
    }
    return status;
}

void tracked_pugl_free_view(PuglView* view) {
    x11_focus_records().erase(view);
    ::puglFreeView(view);
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

#  define puglSetEventFunc tracked_pugl_set_event_func
#  define puglFreeView tracked_pugl_free_view
#  define puglUpdate tracked_pugl_update
#endif

#include "detail/pugl_skia_view_a.inc"
#include "detail/pugl_skia_view_b.inc"

using ViewCore = detail::ViewCore;

#if defined(_WIN32)
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

#if defined(_WIN32)
#  undef PlatformServices
#endif

#if defined(__linux__)
#  undef puglUpdate
#  undef puglFreeView
#  undef puglSetEventFunc
#endif

} // namespace ui