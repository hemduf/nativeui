

// Platform desktop services factories for standalone and embedded views.
#include "standalone_window.hpp"
#include "embedded_view.hpp"
#if defined(__APPLE__)
#  include "../macos_desktop_services.hpp"
#elif defined(_WIN32)
#  include "../windows_desktop_services.hpp"
#elif defined(__linux__)
#  include "../linux_desktop_services.hpp"
#endif

namespace ui {

EmbeddedView::EmbeddedView(
    UI& ui,
    NativeParentHandle parent,
    Size size,
    std::shared_ptr<DesktopServicesBackend> desktop_services_backend)
    : EmbeddedView(ui, parent, size) {
    desktop_services_backend_ = desktop_services_backend;
}

DesktopServices& StandaloneWindow::desktop_services() {
    if (!desktop_services_) {
#if defined(__APPLE__)
        if (!desktop_services_backend_ && impl_ && impl_->core) {
            desktop_services_backend_ = detail::make_macos_desktop_services_backend(
                impl_->core->native_handle());
        }
#elif defined(_WIN32)
        if (!desktop_services_backend_ && impl_ && impl_->core) {
            desktop_services_backend_ = detail::make_windows_desktop_services_backend(
                impl_->core->native_handle());
        }
#elif defined(__linux__)
        if (!desktop_services_backend_ && impl_ && impl_->application && impl_->core) {
            desktop_services_backend_ = detail::make_linux_desktop_services_backend(
                *impl_->application, dispatcher(), impl_->core->native_handle());
        }
#endif
        desktop_services_ = std::make_unique<DesktopServices>(
            dispatcher(), desktop_services_backend_);
    }
    return *desktop_services_;
}

DesktopServices& EmbeddedView::desktop_services() {
    if (!desktop_services_) {
        desktop_services_ = std::make_unique<DesktopServices>(
            dispatcher(), desktop_services_backend_);
    }
    return *desktop_services_;
}

} // namespace ui
