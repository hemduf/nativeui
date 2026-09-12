#pragma once

#include <nativeui/desktop_services.hpp>
#include <nativeui/window.hpp>

#include <memory>

namespace ui::detail {

[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_macos_desktop_services_backend(NativeViewHandle parent_view);

} // namespace ui::detail
