#pragma once

#include <nativeui/desktop_services.hpp>

#include <cstdint>
#include <memory>

#if defined(NATIVEUI_T064_FAULT_INJECTION)
#include <atomic>
#include <cstddef>
#include <functional>
#endif

namespace ui::detail {

// Keep the internal backend contract independent from window.hpp so isolated
// platform contracts do not pull renderer/Skia headers just to carry a native
// view handle. The public ui::NativeViewHandle is the same uintptr_t boundary.
using NativeViewHandle = std::uintptr_t;

[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_macos_desktop_services_backend(std::uintptr_t parent_view);

#if defined(NATIVEUI_T064_FAULT_INJECTION)
struct MacDesktopServicesFaultHooks final {
    std::function<void()> before_result_processing;
    std::shared_ptr<std::atomic<std::size_t>> released_panels;
};

[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_macos_desktop_services_backend_for_testing(
    std::uintptr_t parent_view,
    MacDesktopServicesFaultHooks hooks);
#endif

} // namespace ui::detail
