#include <nativeui/window.hpp>

// Export one inert platform reference so the final MODULE must pull NativeUI's
// platform C++ object and its consumer-scoped Pugl/Objective-C bridge. The
// loader never calls this function; its only purpose is to make link/load
// coexistence observable without creating host windows in the fixture.
#if defined(_WIN32)
#  define T053_EXPORT __declspec(dllexport)
#else
#  define T053_EXPORT __attribute__((visibility("default")))
#endif

extern "C" T053_EXPORT bool
t053_consumer_platform_anchor(ui::EmbeddedView* view)
{
    return view && view->poll();
}
