#include "application.hpp"

namespace ui {

Application::Application()
    : impl_(std::make_unique<Impl>()) {}
Application::~Application() = default;

bool Application::valid() const noexcept {
    return impl_ && impl_->valid();
}
std::string_view Application::last_error() const noexcept {
    return impl_ ? std::string_view{impl_->last_error} : std::string_view{};
}
int Application::run() {
    if (!impl_ || !impl_->valid()) return 1;
#if defined(__EMSCRIPTEN__)
    // The browser owns one module-global main-loop slot. A second Application
    // remains fully usable through non-blocking poll(); it simply cannot also
    // claim run().
    auto*& browser_owner = Application::Impl::browser_main_loop_owner();
    if (browser_owner) {
        try {
            impl_->last_error = browser_owner == impl_.get()
                ? "Application::run() already owns the Emscripten module main loop"
                : "Another Application::run() owns the Emscripten module main loop; use poll() for additional instances";
        } catch (...) {
        }
        return 1;
    }

    impl_->last_error.clear();
    browser_owner = impl_.get();
    emscripten_set_main_loop_arg(
        [](void* data) {
            auto* impl = static_cast<Application::Impl*>(data);
            if (!impl) {
                emscripten_cancel_main_loop();
                return;
            }

            try {
                if (!impl->pump(0.0)) {
                    impl->stop_browser_main_loop();
                }
            } catch (const std::exception& error) {
                impl->fail_browser_main_loop(error.what());
            } catch (...) {
                impl->fail_browser_main_loop("Unhandled exception in browser main loop");
            }
        },
        impl_.get(),
        0,
        1);
    return 0;
#else
    while (!impl_->quit_requested && !impl_->terminal_error) {
        if (!impl_->pump(-1.0) && !impl_->quit_requested) return 1;
    }
    return impl_->terminal_error ? 1 : 0;
#endif
}
bool Application::poll(double timeout_seconds) {
    return impl_ && impl_->pump(timeout_seconds);
}
void Application::request_quit() {
    if (impl_) impl_->quit_requested = true;
}
bool Application::quit_requested() const noexcept {
    return !impl_ || impl_->quit_requested;
}
void Application::set_quit_policy(QuitPolicy policy) noexcept {
    if (impl_) impl_->quit_policy = policy;
}
QuitPolicy Application::quit_policy() const noexcept {
    return impl_ ? impl_->quit_policy : QuitPolicy::OnLastWindowClosed;
}

} // namespace ui
