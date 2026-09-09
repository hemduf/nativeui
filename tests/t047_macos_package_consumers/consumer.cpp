#include <nativeui/window.hpp>

#if defined(_WIN32)
#  define T047_EXPORT __declspec(dllexport)
#else
#  define T047_EXPORT __attribute__((visibility("default")))
#endif

// Force the final MODULE to retain NativeUI's platform C++ layer and the
// consumer-scoped Pugl/Objective-C bridge without creating a native window.
extern "C" T047_EXPORT bool
t047_consumer_platform_anchor(ui::EmbeddedView* view)
{
    return view && view->poll();
}
