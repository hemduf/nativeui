#include <nativeui/detail/dispatcher_owner.hpp>

#include "detail/pugl_button_translation.hpp"
#include "detail/pugl_pointer_translation.hpp"
#include "detail/pugl_scroll_translation.hpp"
#include "detail/scoped_borrow_state.hpp"
#include "detail/scene_damage.hpp"
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
#include <exception>
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
#include "detail/native_ime_bridge.h"

// Pugl/Skia remains consumer-scoped and is compiled once per final target.
// The internal implementation modules are complete and scoped, not fragments
// of a class definition or switch statement.
#include "detail/platform/view_core.hpp"
#include "detail/platform/native_platform.hpp"

namespace ui {

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
