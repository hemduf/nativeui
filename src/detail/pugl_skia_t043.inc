
namespace ui {

void StandaloneWindow::set_preferred_size_callback(PreferredSizeCallback callback) {
    if (impl_ && impl_->core) impl_->core->set_preferred_size_callback(std::move(callback));
}

void EmbeddedView::set_preferred_size_callback(PreferredSizeCallback callback) {
    if (impl_) impl_->core.set_preferred_size_callback(std::move(callback));
}

} // namespace ui
