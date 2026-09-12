#pragma once

#include <nativeui/desktop_services.hpp>

#include <cstdint>
#include <memory>

namespace ui::detail {

// Keep the internal backend contract independent from window.hpp so isolated
// platform contracts do not pull renderer/Skia headers just to carry a native
// view handle. The public ui::NativeViewHandle is the same uintptr_t boundary.
using NativeViewHandle = std::uintptr_t;

[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_macos_desktop_services_backend(std::uintptr_t parent_view);

} // namespace ui::detail
