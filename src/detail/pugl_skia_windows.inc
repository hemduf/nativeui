struct StandaloneWindow::Impl {
    Impl(UI& ui, PlatformServices& services, WindowDesc desc)
        : core(ui, services, PUGL_PROGRAM, std::move(desc), 0) {}
    ViewCore core;
};

StandaloneWindow::StandaloneWindow(UI& ui, WindowDesc desc)
    : impl_(std::make_unique<Impl>(ui, *this, std::move(desc))) {}
StandaloneWindow::~StandaloneWindow() = default;
StandaloneWindow::StandaloneWindow(StandaloneWindow&&) noexcept = default;
StandaloneWindow& StandaloneWindow::operator=(StandaloneWindow&&) noexcept = default;

int StandaloneWindow::run() {
    while (!should_close()) poll(-1.0);
    return impl_ && impl_->core.last_error().empty() ? 0 : 1;
}
bool StandaloneWindow::poll(double timeout_seconds) { return impl_->core.poll(timeout_seconds); }
void StandaloneWindow::request_close() { impl_->core.request_close(); }
bool StandaloneWindow::should_close() const noexcept { return !impl_ || impl_->core.should_close(); }
Size StandaloneWindow::size() const noexcept { return impl_ ? impl_->core.size() : Size{}; }
float StandaloneWindow::scale_factor() const noexcept { return impl_ ? impl_->core.scale_factor() : 1.0f; }
NativeViewHandle StandaloneWindow::native_handle() const noexcept { return impl_ ? impl_->core.native_handle() : 0; }
std::string_view StandaloneWindow::last_error() const noexcept { return impl_ ? std::string_view{impl_->core.last_error()} : std::string_view{}; }
bool StandaloneWindow::set_size(Size size) { return impl_ && impl_->core.set_size(size); }
void StandaloneWindow::set_text_input(bool active, Rect area, float cursor_offset) {
    impl_->core.set_text_input(active, area, cursor_offset);
}
void StandaloneWindow::set_clipboard_text(std::string_view text) { impl_->core.set_clipboard_text(text); }
void StandaloneWindow::request_clipboard_text() { impl_->core.request_clipboard_text(); }
bool StandaloneWindow::accept_drop(std::string_view type, Rect region) {
    return impl_ && impl_->core.accept_drop(type, region);
}
void StandaloneWindow::reject_drop(Rect region) { if (impl_) impl_->core.reject_drop(region); }

struct EmbeddedView::Impl {
    Impl(UI& ui, PlatformServices& services, NativeParentHandle parent, Size size)
        : core(ui,
               services,
               PUGL_MODULE,
               WindowDesc{"NativeUI Embedded", size, true},
               parent) {}
    ViewCore core;
};

EmbeddedView::EmbeddedView(UI& ui, NativeParentHandle parent, Size size)
    : impl_(std::make_unique<Impl>(ui, *this, parent, size)) {}
EmbeddedView::~EmbeddedView() = default;
EmbeddedView::EmbeddedView(EmbeddedView&&) noexcept = default;
EmbeddedView& EmbeddedView::operator=(EmbeddedView&&) noexcept = default;

bool EmbeddedView::poll() { return impl_->core.poll(0.0); }
void EmbeddedView::request_close() { impl_->core.request_close(); }
bool EmbeddedView::should_close() const noexcept { return !impl_ || impl_->core.should_close(); }
Size EmbeddedView::size() const noexcept { return impl_ ? impl_->core.size() : Size{}; }
float EmbeddedView::scale_factor() const noexcept { return impl_ ? impl_->core.scale_factor() : 1.0f; }
NativeViewHandle EmbeddedView::native_handle() const noexcept { return impl_ ? impl_->core.native_handle() : 0; }
std::string_view EmbeddedView::last_error() const noexcept { return impl_ ? std::string_view{impl_->core.last_error()} : std::string_view{}; }
bool EmbeddedView::set_size(Size size) { return impl_ && impl_->core.set_size(size); }
void EmbeddedView::set_text_input(bool active, Rect area, float cursor_offset) {
    impl_->core.set_text_input(active, area, cursor_offset);
}
void EmbeddedView::set_clipboard_text(std::string_view text) { impl_->core.set_clipboard_text(text); }
void EmbeddedView::request_clipboard_text() { impl_->core.request_clipboard_text(); }
bool EmbeddedView::accept_drop(std::string_view type, Rect region) {
    return impl_ && impl_->core.accept_drop(type, region);
}
void EmbeddedView::reject_drop(Rect region) { if (impl_) impl_->core.reject_drop(region); }

} // namespace ui
