#pragma once

// Standalone window close lifecycle, UI-facing delegation and tests.
#include "application.hpp"
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
#  include "../platform_test_access.hpp"
#endif

namespace ui {

struct StandaloneWindow::Impl final : detail::WindowPlatformServices, DispatcherProvider {
    explicit Impl(std::shared_ptr<detail::DispatcherWakeBackend> wake_backend = {})
        : dispatcher_owner(
              std::make_shared<detail::DispatcherOwner>(std::move(wake_backend))),
          lifetime_token(std::make_shared<int>(0)) {}

    // Declared before `core` so reverse member destruction tears detail::ViewCore down
    // while the T065 dispatcher remains available to component deactivation.
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
    std::unique_ptr<detail::ViewCore> core;
    Application* application{};
    std::uint64_t registration_id{};
    std::string construction_error;
    detail::WindowCloseState close_state;
    std::function<CloseDecision()> close_request_callback;
    std::function<void()> closed_callback;
    std::shared_ptr<int> lifetime_token;
    detail::WindowControlPostState close_request_post;
    detail::WindowControlPostState close_completion_post;
    bool close_request_callback_active{};

    [[nodiscard]] Dispatcher dispatcher() const noexcept override {
        return dispatcher_owner ? dispatcher_owner->dispatcher() : Dispatcher{};
    }

    void set_text_input(bool active, Rect area, float cursor_offset) override {
        if (core) core->set_text_input(active, area, cursor_offset);
    }
    void set_clipboard_text(std::string_view text) override {
        if (core) core->set_clipboard_text(text);
    }
    void request_clipboard_text() override {
        if (core) core->request_clipboard_text();
    }
    bool accept_drop(std::string_view type, Rect region) override {
        return core && core->accept_drop(type, region);
    }
    void reject_drop(Rect region) override {
        if (core) core->reject_drop(region);
    }
};

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
bool detail::PlatformTestAccess::request_gpu_readback(
    StandaloneWindow& window,
    Point logical_point) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->request_gpu_readback(logical_point);
}

std::optional<detail::PlatformReadbackPixel>
detail::PlatformTestAccess::take_gpu_readback(
    StandaloneWindow& window) noexcept {
    if (!window.impl_ || !window.impl_->core) return std::nullopt;
    return window.impl_->core->take_gpu_readback();
}

bool detail::PlatformTestAccess::request_gpu_readback_region(
    StandaloneWindow& window,
    int x,
    int y,
    int width,
    int height) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->request_gpu_readback_region(x, y, width, height);
}

std::optional<detail::PlatformReadbackRegion>
detail::PlatformTestAccess::take_gpu_readback_region(
    StandaloneWindow& window) noexcept {
    if (!window.impl_ || !window.impl_->core) return std::nullopt;
    return window.impl_->core->take_gpu_readback_region();
}

bool detail::PlatformTestAccess::suppress_platform_focus(
    StandaloneWindow& window, bool suppressed) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->suppress_platform_focus(suppressed);
}

bool detail::PlatformTestAccess::inject_scene_fault(
    StandaloneWindow& window,
    detail::SceneFaultStage stage) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->inject_scene_fault(stage);
}

detail::SceneDiagnostics detail::PlatformTestAccess::scene_diagnostics(
    StandaloneWindow& window) noexcept {
    if (!window.impl_ || !window.impl_->core) return {};
    return window.impl_->core->scene_diagnostics();
}

bool detail::PlatformTestAccess::request_context_recreation(
    StandaloneWindow& window) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->request_context_recreation();
}

bool detail::PlatformTestAccess::override_scene_scale(
    StandaloneWindow& window,
    std::optional<float> scale) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->override_scene_scale(scale);
}

bool detail::PlatformTestAccess::reject_next_deferred_redraw(
    StandaloneWindow& window) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->reject_next_deferred_redraw();
}

bool detail::PlatformTestAccess::request_expose(
    StandaloneWindow& window) noexcept {
    return window.impl_ && window.impl_->core &&
           window.impl_->core->request_expose();
}

#endif
StandaloneWindow::StandaloneWindow(Application& application, UI& ui, WindowDesc desc)
    : impl_(std::make_unique<Impl>(
          application.impl_ ? application.impl_->wake_backend : nullptr)) {
    if (!application.impl_ || !application.impl_->runnable()) {
        impl_->construction_error = application.last_error().empty()
            ? "Application is not runnable"
            : std::string{application.last_error()};
        impl_->dispatcher_owner->shutdown();
        return;
    }

    impl_->application = &application;
    try {
        impl_->core = std::make_unique<detail::ViewCore>(
            ui,
            *impl_,
            PUGL_PROGRAM,
            std::move(desc),
            0,
            application.impl_->world,
            [this] { handle_native_close_request(); });
        const std::weak_ptr<int> lifetime = impl_->lifetime_token;
        impl_->registration_id = application.impl_->register_window(
            impl_->dispatcher_owner,
            lifetime,
            this,
            [](void* context) {
                auto* self = static_cast<StandaloneWindow*>(context);
                if (!self || !self->impl_ || !self->impl_->core) return;

                const std::weak_ptr<int> window_lifetime = self->impl_->lifetime_token;
                if (self->impl_->close_state.requesting()) {
                    self->impl_->close_request_post.cancel();
                    self->process_native_close_request();
                    if (window_lifetime.expired()) return;
                }
                if (self->impl_->close_state.pending()) {
                    self->impl_->close_completion_post.cancel();
                    self->complete_close();
                }
            });
        if (!impl_->registration_id) {
            impl_->construction_error = application.last_error().empty()
                ? "Application rejected window registration"
                : std::string{application.last_error()};
            impl_->dispatcher_owner->shutdown();
            impl_->core.reset();
            impl_->application = nullptr;
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        impl_->construction_error = error.what();
        impl_->dispatcher_owner->shutdown();
        impl_->core.reset();
        impl_->application = nullptr;
    }
}

StandaloneWindow::StandaloneWindow(UI& ui, WindowDesc desc)
    : impl_(std::make_unique<Impl>()) {
    impl_->core = std::make_unique<detail::ViewCore>(
        ui,
        *impl_,
        PUGL_PROGRAM,
        std::move(desc),
        0,
        nullptr,
        [this] { handle_native_close_request(); });
}

StandaloneWindow::~StandaloneWindow() {
    if (!impl_) return;
    impl_->close_state.begin_teardown();
    impl_->close_request_post.cancel();
    impl_->close_completion_post.cancel();
    impl_->lifetime_token.reset();
    impl_->close_request_callback = {};
    impl_->closed_callback = {};
    if (impl_->core) (void)impl_->core->close_native_view();

    if (impl_->application) {
        unregister_from_application();
    } else if (impl_->dispatcher_owner) {
        impl_->dispatcher_owner->shutdown();
    }
}

void StandaloneWindow::handle_native_close_request() {
    if (!impl_ || !impl_->core || !impl_->close_state.begin_user_request()) return;

    const auto dispatcher = impl_->dispatcher_owner
        ? impl_->dispatcher_owner->dispatcher()
        : Dispatcher{};
    const std::weak_ptr<int> lifetime = impl_->lifetime_token;
    auto* self = this;
    const bool posted = impl_->close_request_post.try_post([&] {
        return dispatcher.post([lifetime, self] {
            if (lifetime.expired()) return;
            self->impl_->close_request_post.callback_started();
            self->process_native_close_request();
        });
    });

    // Rejection/exception leaves Requesting as durable lifecycle work. The
    // Application checkpoint runs after native dispatch and Dispatcher drain;
    // request a platform wake only for the direct-call case where no user task
    // was accepted to provide one.
    if (!posted && impl_->application && impl_->application->impl_ &&
        impl_->application->impl_->wake_backend) {
        impl_->application->impl_->wake_backend->request_wake();
    }
}

void StandaloneWindow::process_native_close_request() {
    if (!impl_ || !impl_->core || !impl_->close_state.requesting()) return;

    impl_->close_request_post.cancel();
    impl_->close_request_callback_active = true;
    const std::weak_ptr<int> lifetime = impl_->lifetime_token;
    const auto callback = impl_->close_request_callback;
    CloseDecision decision = CloseDecision::Accept;
    if (callback) {
        try {
            decision = callback();
        } catch (...) {
            decision = CloseDecision::Cancel;
        }
    }

    // A callback is allowed to destroy this window. The weak token is local and
    // remains valid to inspect after destruction, so do not touch `this` again
    // when the object lifetime ended inside user code.
    if (lifetime.expired()) return;
    impl_->close_request_callback_active = false;
    impl_->close_state.finish_user_request(decision == CloseDecision::Accept);
    if (impl_->close_state.pending()) schedule_close_completion();
}

void StandaloneWindow::schedule_close_completion() {
    if (!impl_ || !impl_->core || !impl_->close_state.pending() ||
        impl_->close_completion_post.posted()) {
        return;
    }

    const auto dispatcher = impl_->dispatcher_owner
        ? impl_->dispatcher_owner->dispatcher()
        : Dispatcher{};
    const std::weak_ptr<int> lifetime = impl_->lifetime_token;
    auto* self = this;
    const bool posted = impl_->close_completion_post.try_post([&] {
        return dispatcher.post([lifetime, self] {
            if (lifetime.expired()) return;
            self->impl_->close_completion_post.callback_started();
            self->complete_close();
        });
    });

    // Never fall back to synchronous teardown on an active callback stack.
    // Pending is durable and will be consumed by the owner/platform checkpoint.
    if (!posted && impl_->application && impl_->application->impl_ &&
        impl_->application->impl_->wake_backend) {
        impl_->application->impl_->wake_backend->request_wake();
    }
}

void StandaloneWindow::complete_close() {
    if (!impl_ || !impl_->core) return;
    impl_->close_request_post.cancel();
    impl_->close_completion_post.cancel();
    if (!impl_->close_state.complete_accepted_close()) return;

    (void)impl_->core->close_native_view();
    if (impl_->application) {
        mark_application_window_closed();
    } else if (impl_->dispatcher_owner) {
        impl_->dispatcher_owner->shutdown();
    }

    const auto callback = impl_->closed_callback;
    if (callback) callback();
}

void StandaloneWindow::mark_application_window_closed() noexcept {
    if (!impl_ || !impl_->application || !impl_->registration_id) return;
    auto* application = impl_->application;
    if (application->impl_) {
        application->impl_->mark_window_closed(impl_->registration_id);
    }
}

void StandaloneWindow::unregister_from_application() noexcept {
    if (!impl_ || !impl_->application || !impl_->registration_id) return;
    auto* application = impl_->application;
    if (application->impl_) {
        application->impl_->unregister_window(impl_->registration_id);
    }
    impl_->registration_id = 0;
    impl_->application = nullptr;
}

int StandaloneWindow::run() {
    if (!impl_) return 1;
    if (impl_->application) return impl_->application->run();
    while (impl_ && impl_->core && !is_closed() && !impl_->core->should_close()) {
        (void)poll(-1.0);
    }
    return valid() || is_closed() ? 0 : 1;
}
bool StandaloneWindow::poll(double timeout_seconds) {
    if (!impl_) return false;
    if (impl_->application) return impl_->application->poll(timeout_seconds);
    if (!impl_->core) return false;

    const auto owner = impl_->dispatcher_owner;
    const std::weak_ptr<int> lifetime = impl_->lifetime_token;
    std::vector<std::shared_ptr<detail::DispatcherOwner>> owners;
    if (owner) owners.push_back(owner);
    const bool close_control_ready =
        impl_->close_state.requesting() || impl_->close_state.pending();
    const double requested = close_control_ready
        ? 0.0
        : (timeout_seconds < 0.0 ? -1.0 : timeout_seconds);
    const double pugl_timeout =
        platform_detail::bound_dispatch_timeout(requested, owners);
    const bool alive = impl_->core->poll(pugl_timeout);
    if (owner) (void)owner->checkpoint();
    if (lifetime.expired()) return false;

    if (impl_->close_state.requesting()) {
        impl_->close_request_post.cancel();
        process_native_close_request();
        if (lifetime.expired()) return false;
    }
    if (impl_->close_state.pending()) {
        impl_->close_completion_post.cancel();
        complete_close();
        if (lifetime.expired()) return false;
    }
    return alive && !is_closed();
}
void StandaloneWindow::request_close() {
    if (!impl_ || !impl_->core) return;
    if (impl_->close_state.request_programmatic()) schedule_close_completion();
}
bool StandaloneWindow::valid() const noexcept {
    return impl_ && impl_->core && impl_->construction_error.empty() &&
           impl_->core->native_view_open();
}
bool StandaloneWindow::should_close() const noexcept {
    return !impl_ || !impl_->core || impl_->close_state.pending() ||
           is_closed() || impl_->core->should_close();
}
bool StandaloneWindow::is_closed() const noexcept {
    return impl_ && impl_->close_state.closed();
}
Size StandaloneWindow::size() const noexcept {
    return impl_ && impl_->core ? impl_->core->size() : Size{};
}
float StandaloneWindow::scale_factor() const noexcept {
    return impl_ && impl_->core ? impl_->core->scale_factor() : 1.0f;
}
NativeViewHandle StandaloneWindow::native_handle() const noexcept {
    return impl_ && impl_->core ? impl_->core->native_handle() : 0;
}
std::string_view StandaloneWindow::last_error() const noexcept {
    if (!impl_) return {};
    if (!impl_->construction_error.empty()) return impl_->construction_error;
    return impl_->core ? std::string_view{impl_->core->last_error()} : std::string_view{};
}
Dispatcher StandaloneWindow::dispatcher() const noexcept {
    return impl_ && impl_->core && impl_->dispatcher_owner
        ? impl_->dispatcher_owner->dispatcher()
        : Dispatcher{};
}
bool StandaloneWindow::set_title(std::string_view title) {
    return impl_ && impl_->core && !is_closed() && impl_->core->set_title(title);
}
bool StandaloneWindow::show() {
    return impl_ && impl_->core && !is_closed() && impl_->core->show();
}
bool StandaloneWindow::hide() {
    return impl_ && impl_->core && !is_closed() && impl_->core->hide();
}
bool StandaloneWindow::set_size(Size size) {
    return impl_ && impl_->core && !is_closed() && impl_->core->set_size(size);
}
bool StandaloneWindow::set_min_size(std::optional<Size> size) {
    return impl_ && impl_->core && !is_closed() && impl_->core->set_min_size(size);
}
bool StandaloneWindow::set_max_size(std::optional<Size> size) {
    return impl_ && impl_->core && !is_closed() && impl_->core->set_max_size(size);
}
void StandaloneWindow::on_close_request(std::function<CloseDecision()> callback) {
    if (!impl_ || is_closed()) return;
    impl_->close_request_callback = std::move(callback);
}
void StandaloneWindow::on_closed(std::function<void()> callback) {
    if (!impl_ || is_closed()) return;
    impl_->closed_callback = std::move(callback);
}
void StandaloneWindow::set_text_input(bool active, Rect area, float cursor_offset) {
    if (impl_ && impl_->core && !is_closed()) impl_->core->set_text_input(active, area, cursor_offset);
}
void StandaloneWindow::set_clipboard_text(std::string_view text) {
    if (impl_ && impl_->core && !is_closed()) impl_->core->set_clipboard_text(text);
}
void StandaloneWindow::request_clipboard_text() {
    if (impl_ && impl_->core && !is_closed()) impl_->core->request_clipboard_text();
}
bool StandaloneWindow::accept_drop(std::string_view type, Rect region) {
    return impl_ && impl_->core && !is_closed() && impl_->core->accept_drop(type, region);
}
void StandaloneWindow::reject_drop(Rect region) {
    if (impl_ && impl_->core && !is_closed()) impl_->core->reject_drop(region);
}


void StandaloneWindow::set_preferred_size_callback(PreferredSizeCallback callback) {
    if (impl_ && impl_->core) impl_->core->set_preferred_size_callback(std::move(callback));
}

} // namespace ui
