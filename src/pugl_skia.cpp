#include <nativeui/detail/dispatcher_owner.hpp>

#if defined(__APPLE__)
#  include <CoreFoundation/CFRunLoop.h>
#elif defined(__linux__)
struct _XDisplay;
extern "C" int XFlush(_XDisplay* display);
#endif

#include <mutex>
#include <vector>

#include "detail/native_ime_bridge.h"
#include "detail/pugl_skia_setup.inc"
#include "detail/pugl_skia_show_policy.inc"
#include "detail/pugl_skia_view_a.inc"
#include "detail/pugl_skia_view_b.inc"
#include "detail/pugl_skia_windows.inc"
