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
