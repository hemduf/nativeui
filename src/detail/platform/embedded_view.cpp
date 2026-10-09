#include "embedded_view.hpp"

namespace ui {

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
bool detail::PlatformTestAccess::request_gpu_readback(
    EmbeddedView& view,
    Point logical_point) noexcept {
    return view.impl_ && view.impl_->core.request_gpu_readback(logical_point);
}

std::optional<detail::PlatformReadbackPixel>
detail::PlatformTestAccess::take_gpu_readback(EmbeddedView& view) noexcept {
    return view.impl_ ? view.impl_->core.take_gpu_readback() : std::nullopt;
}

bool detail::PlatformTestAccess::inject_scene_fault(
    EmbeddedView& view,
    detail::SceneFaultStage stage) noexcept {
    return view.impl_ && view.impl_->core.inject_scene_fault(stage);
}

detail::SceneDiagnostics detail::PlatformTestAccess::scene_diagnostics(
    EmbeddedView& view) noexcept {
    return view.impl_ ? view.impl_->core.scene_diagnostics() : detail::SceneDiagnostics{};
}
#endif

EmbeddedView::EmbeddedView(UI& ui, NativeParentHandle parent, Size size)
    : EmbeddedView(ui, parent, size, {}, {}) {}
EmbeddedView::EmbeddedView(UI& ui, NativeParentHandle parent, Size size,
                           std::shared_ptr<DesktopServicesBackend> backend,
                           EmbeddedViewOptions options)
    : impl_(std::make_unique<Impl>(ui, parent, size, options)),
      desktop_services_backend_(std::move(backend)) {}
EmbeddedView::~EmbeddedView() = default;

bool EmbeddedView::poll() {
    if (!impl_) return false;
    const auto owner = impl_->dispatcher_owner;
    const bool alive = impl_->core.poll(0.0);
    if (owner) (void)owner->checkpoint();
    return alive;
}
void EmbeddedView::request_close() {
    if (!impl_) return;
    impl_->core.request_close();
    if (impl_->dispatcher_owner) impl_->dispatcher_owner->shutdown();
}
bool EmbeddedView::should_close() const noexcept { return !impl_ || impl_->core.should_close(); }
bool EmbeddedView::show() { return impl_ && impl_->core.show(); }
bool EmbeddedView::hide() { return impl_ && impl_->core.hide(); }
bool EmbeddedView::visible() const noexcept { return impl_ && impl_->core.visible(); }
Size EmbeddedView::size() const noexcept { return impl_ ? impl_->core.size() : Size{}; }
float EmbeddedView::scale_factor() const noexcept { return impl_ ? impl_->core.scale_factor() : 1.0f; }
NativeViewHandle EmbeddedView::native_handle() const noexcept { return impl_ ? impl_->core.native_handle() : 0; }
std::string_view EmbeddedView::last_error() const noexcept { return impl_ ? std::string_view{impl_->core.last_error()} : std::string_view{}; }
Dispatcher EmbeddedView::dispatcher() const noexcept {
    return impl_ && impl_->dispatcher_owner ? impl_->dispatcher_owner->dispatcher() : Dispatcher{};
}
bool EmbeddedView::set_size(Size size) { return impl_ && impl_->core.set_size(size); }
void EmbeddedView::set_text_input(bool active, Rect area, float cursor_offset) {
    if (impl_) impl_->core.set_text_input(active, area, cursor_offset);
}
void EmbeddedView::set_clipboard_text(std::string_view text) {
    if (impl_) impl_->core.set_clipboard_text(text);
}
void EmbeddedView::request_clipboard_text() {
    if (impl_) impl_->core.request_clipboard_text();
}
bool EmbeddedView::accept_drop(std::string_view type, Rect region) {
    return impl_ && impl_->core.accept_drop(type, region);
}
void EmbeddedView::reject_drop(Rect region) {
    if (impl_) impl_->core.reject_drop(region);
}


void EmbeddedView::set_preferred_size_callback(PreferredSizeCallback callback) {
    if (impl_) impl_->core.set_preferred_size_callback(std::move(callback));
}

} // namespace ui
