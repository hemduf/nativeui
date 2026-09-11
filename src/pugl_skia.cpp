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

} // namespace ui