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

#include "detail/pugl_skia_windows.inc"

} // namespace ui
