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
#if defined(__linux__)
        if (!desktop_services_backend_ && impl_ && impl_->application) {
            desktop_services_backend_ = detail::make_linux_desktop_services_backend(
                *impl_->application, dispatcher());
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
