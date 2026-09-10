struct Application::Impl final {
    PuglWorld* world{};
    QuitPolicy quit_policy{QuitPolicy::OnLastWindowClosed};
    bool quit_requested{};
    bool terminal_error{};
    std::uint64_t next_window_id{1};
    std::size_t open_window_count{};
    std::unordered_map<std::uint64_t, bool> windows;
    std::string last_error;

    Impl() {
        world = puglNewWorld(PUGL_PROGRAM, 0);
        if (!world) {
            terminal_error = true;
            last_error = "puglNewWorld failed";
            return;
        }
        puglSetWorldString(world, PUGL_CLASS_NAME, "NativeUI");
    }

    ~Impl() {
        // Application must outlive every explicitly attached StandaloneWindow.
        // Terminating here is deliberate: freeing the PROGRAM world while a
        // borrowed view is still alive would turn a clear lifetime violation
        // into a platform UAF during later window teardown.
        if (!windows.empty()) std::terminate();
        if (world) puglFreeWorld(world);
    }

    [[nodiscard]] bool valid() const noexcept {
        return world && !terminal_error;
    }

    [[nodiscard]] bool runnable() const noexcept {
        return valid() && !quit_requested;
    }

    std::uint64_t register_window() {
        if (!runnable() || next_window_id == 0) return 0;
        const auto id = next_window_id++;
        windows.emplace(id, true);
        ++open_window_count;
        return id;
    }

    void mark_window_closed(std::uint64_t id) noexcept {
        const auto found = windows.find(id);
        if (found == windows.end() || !found->second) return;
        found->second = false;
        if (open_window_count) --open_window_count;
        if (open_window_count == 0 && quit_policy == QuitPolicy::OnLastWindowClosed) {
            quit_requested = true;
        }
    }

    void unregister_window(std::uint64_t id) noexcept {
        const auto found = windows.find(id);
        if (found == windows.end()) return;
        if (found->second) mark_window_closed(id);
        windows.erase(id);
    }

    bool pump(double timeout_seconds) {
        if (!runnable()) return false;
        if (!std::isfinite(timeout_seconds)) {
            terminal_error = true;
            quit_requested = true;
            last_error = "Application poll timeout must be finite";
            return false;
        }

        const double pugl_timeout = timeout_seconds < 0.0 ? -1.0 : timeout_seconds;
        const auto status = puglUpdate(world, pugl_timeout);
        if (status != PUGL_SUCCESS) {
            terminal_error = true;
            quit_requested = true;
            last_error = puglStrerror(status);
            return false;
        }
        return !quit_requested;
    }
};

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
    while (!impl_->quit_requested && !impl_->terminal_error) {
        if (!impl_->pump(-1.0) && !impl_->quit_requested) return 1;
    }
    return impl_->terminal_error ? 1 : 0;
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

struct StandaloneWindow::Impl final : PlatformServices {
    std::unique_ptr<ViewCore> core;
    Application* application{};
    std::uint64_t registration_id{};
    std::string construction_error;
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner{
        std::make_shared<detail::DispatcherOwner>()};

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

StandaloneWindow::StandaloneWindow(Application& application, UI& ui, WindowDesc desc)
    : impl_(std::make_unique<Impl>()) {
    if (!application.impl_ || !application.impl_->runnable()) {
        impl_->construction_error = application.last_error().empty()
            ? "Application is not runnable"
            : std::string{application.last_error()};
        return;
    }

    impl_->application = &application;
    try {
        impl_->core = std::make_unique<ViewCore>(
            ui,
            *impl_,
            PUGL_PROGRAM,
            std::move(desc),
            0,
            application.impl_->world,
            [this] { mark_application_window_closed(); });
        impl_->registration_id = application.impl_->register_window();
        if (!impl_->registration_id) {
            impl_->construction_error = "Application rejected window registration";
            impl_->core.reset();
            impl_->application = nullptr;
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        impl_->construction_error = error.what();
        impl_->core.reset();
        impl_->application = nullptr;
    }
}

StandaloneWindow::StandaloneWindow(UI& ui, WindowDesc desc)
    : impl_(std::make_unique<Impl>()) {
    impl_->core = std::make_unique<ViewCore>(ui, *impl_, PUGL_PROGRAM, std::move(desc), 0);
}

StandaloneWindow::~StandaloneWindow() {
    unregister_from_application();
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
    while (!should_close()) poll(-1.0);
    return valid() ? 0 : 1;
}
bool StandaloneWindow::poll(double timeout_seconds) {
    if (!impl_) return false;
    if (impl_->application) return impl_->application->poll(timeout_seconds);
    return impl_->core && impl_->core->poll(timeout_seconds);
}
void StandaloneWindow::request_close() {
    if (!impl_ || !impl_->core) return;
    impl_->core->request_close();
    mark_application_window_closed();
}
bool StandaloneWindow::valid() const noexcept {
    return impl_ && impl_->core && impl_->construction_error.empty() &&
           impl_->core->last_error().empty();
}
bool StandaloneWindow::should_close() const noexcept {
    return !impl_ || !impl_->core || impl_->core->should_close();
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
bool StandaloneWindow::set_size(Size size) {
    return impl_ && impl_->core && impl_->core->set_size(size);
}
void StandaloneWindow::set_text_input(bool active, Rect area, float cursor_offset) {
    if (impl_ && impl_->core) impl_->core->set_text_input(active, area, cursor_offset);
}
void StandaloneWindow::set_clipboard_text(std::string_view text) {
    if (impl_ && impl_->core) impl_->core->set_clipboard_text(text);
}
void StandaloneWindow::request_clipboard_text() {
    if (impl_ && impl_->core) impl_->core->request_clipboard_text();
}
bool StandaloneWindow::accept_drop(std::string_view type, Rect region) {
    return impl_ && impl_->core && impl_->core->accept_drop(type, region);
}
void StandaloneWindow::reject_drop(Rect region) {
    if (impl_ && impl_->core) impl_->core->reject_drop(region);
}

struct EmbeddedView::Impl final : PlatformServices {
    Impl(UI& ui, NativeParentHandle parent, Size size)
        : core(ui,
               *this,
               PUGL_MODULE,
               WindowDesc{"NativeUI Embedded", size, true},
               parent) {}

    void set_text_input(bool active, Rect area, float cursor_offset) override {
        core.set_text_input(active, area, cursor_offset);
    }
    void set_clipboard_text(std::string_view text) override { core.set_clipboard_text(text); }
    void request_clipboard_text() override { core.request_clipboard_text(); }
    bool accept_drop(std::string_view type, Rect region) override {
        return core.accept_drop(type, region);
    }
    void reject_drop(Rect region) override { core.reject_drop(region); }

    ViewCore core;
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner{
        std::make_shared<detail::DispatcherOwner>()};
};

EmbeddedView::EmbeddedView(UI& ui, NativeParentHandle parent, Size size)
    : impl_(std::make_unique<Impl>(ui, parent, size)) {}
EmbeddedView::~EmbeddedView() = default;

bool EmbeddedView::poll() { return impl_->core.poll(0.0); }
void EmbeddedView::request_close() { impl_->core.request_close(); }
bool EmbeddedView::should_close() const noexcept { return !impl_ || impl_->core.should_close(); }
Size EmbeddedView::size() const noexcept { return impl_ ? impl_->core.size() : Size{}; }
float EmbeddedView::scale_factor() const noexcept { return impl_ ? impl_->core.scale_factor() : 1.0f; }
NativeViewHandle EmbeddedView::native_handle() const noexcept { return impl_ ? impl_->core.native_handle() : 0; }
std::string_view EmbeddedView::last_error() const noexcept { return impl_ ? std::string_view{impl_->core.last_error()} : std::string_view{}; }
Dispatcher EmbeddedView::dispatcher() const noexcept {
    return impl_ && impl_->dispatcher_owner ? impl_->dispatcher_owner->dispatcher() : Dispatcher{};
}
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
