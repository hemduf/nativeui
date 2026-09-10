namespace {

class PuglDispatcherWakeBackend final : public detail::DispatcherWakeBackend {
public:
    explicit PuglDispatcherWakeBackend(PuglWorld* world) noexcept
        : world_(world) {}

    ~PuglDispatcherWakeBackend() override = default;

    [[nodiscard]] bool ensure_view() {
        std::lock_guard lock{mutex_};
        if (closing_) return false;
        if (view_) return true;
        if (!world_) return false;

        view_ = puglNewView(world_);
        if (!view_) {
            last_error_ = "puglNewView failed for dispatcher wake bridge";
            return false;
        }

        puglSetViewString(view_, PUGL_WINDOW_TITLE, "NativeUI dispatcher wake bridge");
        puglSetSizeHint(view_, PUGL_DEFAULT_SIZE, 1, 1);
        puglSetBackend(view_, puglGlBackend());
        puglSetEventFunc(view_, [](PuglView*, const PuglEvent*) noexcept {
            return PUGL_SUCCESS;
        });

        const auto status = puglRealize(view_);
        if (status != PUGL_SUCCESS) {
            last_error_ = std::string{"dispatcher wake bridge realization failed: "} +
                          puglStrerror(status);
            puglFreeView(view_);
            view_ = nullptr;
            return false;
        }

#if defined(__APPLE__)
        run_loop_ = CFRunLoopGetMain();
        CFRunLoopSourceContext context{};
        context.info = this;
        context.perform = [](void*) {
            // Break only the innermost blocking run-loop wait. Application
            // immediately re-enters its next pump after dispatcher draining.
            CFRunLoopStop(CFRunLoopGetCurrent());
        };
        wake_source_ = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
        if (!wake_source_) {
            last_error_ = "CFRunLoopSourceCreate failed for dispatcher wake bridge";
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
            run_loop_ = nullptr;
            return false;
        }
        CFRunLoopAddSource(run_loop_, wake_source_, kCFRunLoopCommonModes);
#elif defined(_WIN32)
        native_window_ = reinterpret_cast<HWND>(puglGetNativeView(view_));
        if (!native_window_) {
            last_error_ = "dispatcher wake bridge has no native HWND";
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
            return false;
        }
#elif defined(__linux__)
        display_ = static_cast<Display*>(puglGetNativeWorld(world_));
        native_window_ = static_cast<Window>(puglGetNativeView(view_));
        if (!display_ || !native_window_) {
            last_error_ = "dispatcher wake bridge has no native X11 handle";
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
            display_ = nullptr;
            native_window_ = 0;
            return false;
        }
        wake_atom_ = XInternAtom(display_, "_NATIVEUI_DISPATCHER_WAKE", False);
        if (!wake_atom_) {
            last_error_ = "XInternAtom failed for dispatcher wake bridge";
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
            display_ = nullptr;
            native_window_ = 0;
            return false;
        }
#endif
        return true;
    }

    [[nodiscard]] std::string_view last_error() const noexcept {
        return last_error_;
    }

    void request_wake() noexcept override {
        std::lock_guard lock{mutex_};
        if (closing_ || !view_) return;

        // Worker threads must not call Pugl APIs: PUGL_WORLD_THREADS only makes
        // the Xlib connection thread-aware and does not make Pugl world/view
        // state itself concurrently accessible. Use only platform primitives
        // whose native handles were captured on the UI thread at realization.
#if defined(__APPLE__)
        if (wake_source_ && run_loop_) {
            CFRunLoopSourceSignal(wake_source_);
            CFRunLoopWakeUp(run_loop_);
        }
#elif defined(_WIN32)
        if (native_window_) {
            constexpr UINT kDispatcherWakeMessage = WM_APP + 0x65u;
            (void)PostMessageW(native_window_, kDispatcherWakeMessage, 0, 0);
        }
#elif defined(__linux__)
        if (display_ && native_window_ && wake_atom_) {
            XEvent event{};
            event.xclient.type = ClientMessage;
            event.xclient.display = display_;
            event.xclient.window = native_window_;
            event.xclient.message_type = wake_atom_;
            event.xclient.format = 32;
            (void)XSendEvent(display_, native_window_, False, NoEventMask, &event);
            (void)XFlush(display_);
        }
#endif
    }

    [[nodiscard]] PuglStatus update_world(double timeout_seconds) noexcept {
        PuglWorld* world = nullptr;
#if defined(__linux__)
        Display* display = nullptr;
#endif
        {
            std::lock_guard lock{mutex_};
            if (closing_ || !world_) return PUGL_FAILURE;
            world = world_;
#if defined(__linux__)
            display = display_;
#endif
        }

        if (timeout_seconds == 0.0) return puglUpdate(world, 0.0);

        // Do not use a Pugl repeating timer as a one-shot wait deadline here.
        // X11 timers are optional (XSync-dependent), and Cocoa's NSEvent wait
        // is not interrupted merely because an NSTimer/CF source fired. Wait on
        // each platform's native event primitive, which the worker wake path
        // above can interrupt, then let one non-blocking Pugl update own all
        // actual Pugl world/view dispatch on the UI thread.
#if defined(__APPLE__)
        constexpr double kIndefiniteRunLoopSliceSeconds = 24.0 * 60.0 * 60.0;
        const double wait_seconds =
            timeout_seconds < 0.0 ? kIndefiniteRunLoopSliceSeconds : timeout_seconds;
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, wait_seconds, true);
        return puglUpdate(world, 0.0);
#elif defined(_WIN32)
        DWORD wait_milliseconds = INFINITE;
        if (timeout_seconds >= 0.0) {
            const double milliseconds = std::ceil(timeout_seconds * 1000.0);
            wait_milliseconds = static_cast<DWORD>(std::clamp(
                milliseconds, 1.0, static_cast<double>(INFINITE - 1U)));
        }
        const DWORD wait_result = MsgWaitForMultipleObjectsEx(
            0, nullptr, wait_milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait_result == WAIT_FAILED) return PUGL_FAILURE;
        return puglUpdate(world, 0.0);
#elif defined(__linux__)
        if (!display) return PUGL_FAILURE;

        // Xlib can already own decoded events even when the connection fd is
        // no longer readable, so only block in poll() when its local queue is
        // empty. PUGL_WORLD_THREADS initializes Xlib for the worker XSendEvent
        // wake used by request_wake().
        if (XPending(display) == 0) {
            const int connection = ConnectionNumber(display);
            if (connection < 0) return PUGL_FAILURE;

            int wait_milliseconds = -1;
            if (timeout_seconds >= 0.0) {
                const double milliseconds = std::ceil(timeout_seconds * 1000.0);
                wait_milliseconds = static_cast<int>(std::clamp(
                    milliseconds, 1.0, static_cast<double>(INT_MAX)));
            }

            pollfd descriptor{connection, POLLIN, 0};
            int result = 0;
            do {
                result = ::poll(&descriptor, 1, wait_milliseconds);
            } while (result < 0 && errno == EINTR);
            if (result < 0) return PUGL_FAILURE;
        }
        return puglUpdate(world, 0.0);
#else
        return puglUpdate(world, timeout_seconds);
#endif
    }

    void begin_shutdown() noexcept {
        std::lock_guard lock{mutex_};
        closing_ = true;
    }

    void destroy_view_on_ui_thread() noexcept {
        std::lock_guard lock{mutex_};
#if defined(__APPLE__)
        if (wake_source_) {
            if (run_loop_) {
                CFRunLoopRemoveSource(run_loop_, wake_source_, kCFRunLoopCommonModes);
            }
            CFRelease(wake_source_);
            wake_source_ = nullptr;
        }
        run_loop_ = nullptr;
#elif defined(_WIN32)
        native_window_ = nullptr;
#elif defined(__linux__)
        wake_atom_ = 0;
        native_window_ = 0;
        display_ = nullptr;
#endif
        if (!view_) return;
        (void)puglUnrealize(view_);
        puglFreeView(view_);
        view_ = nullptr;
        world_ = nullptr;
    }

private:
    mutable std::mutex mutex_;
    PuglWorld* world_{};
    PuglView* view_{};
#if defined(__APPLE__)
    CFRunLoopRef run_loop_{};
    CFRunLoopSourceRef wake_source_{};
#elif defined(_WIN32)
    HWND native_window_{};
#elif defined(__linux__)
    Display* display_{};
    Window native_window_{};
    Atom wake_atom_{};
#endif
    bool closing_{};
    std::string last_error_;
};

struct ApplicationWindowRecord final {
    bool open{true};
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
};

[[nodiscard]] double bound_dispatch_timeout(
    double requested,
    const std::vector<std::shared_ptr<detail::DispatcherOwner>>& owners) noexcept {
    double result = requested < 0.0 ? -1.0 : requested;
    for (const auto& owner : owners) {
        if (!owner) continue;
        const auto next = owner->next_delay();
        if (!next) continue;
        const double seconds = std::max(0.0, next->count());
        if (result < 0.0 || seconds < result) result = seconds;
    }
    return result;
}

} // namespace

struct Application::Impl final {
    PuglWorld* world{};
    std::shared_ptr<PuglDispatcherWakeBackend> wake_backend;
    QuitPolicy quit_policy{QuitPolicy::OnLastWindowClosed};
    bool quit_requested{};
    bool terminal_error{};
    std::uint64_t next_window_id{1};
    std::size_t open_window_count{};
    std::unordered_map<std::uint64_t, ApplicationWindowRecord> windows;
    std::string last_error;

    Impl() {
        // Dispatcher worker posts use the platform wake primitive captured by
        // PuglDispatcherWakeBackend. X11 requires XInitThreads before any other
        // Xlib call, which Pugl performs for this PROGRAM world flag.
        world = puglNewWorld(PUGL_PROGRAM, PUGL_WORLD_THREADS);
        if (!world) {
            terminal_error = true;
            last_error = "puglNewWorld failed";
            return;
        }
        puglSetWorldString(world, PUGL_CLASS_NAME, "NativeUI");
        wake_backend = std::make_shared<PuglDispatcherWakeBackend>(world);
    }

    ~Impl() {
        // Application must outlive every explicitly attached StandaloneWindow.
        // Terminating here is deliberate: freeing the PROGRAM world while a
        // borrowed view is still alive would turn a clear lifetime violation
        // into a platform UAF during later window teardown.
        if (!windows.empty()) std::terminate();

        // Stop worker-originated wakes first, process a final non-blocking
        // native checkpoint, then detach the wake source/window before freeing
        // the shared PROGRAM world.
        if (wake_backend) wake_backend->begin_shutdown();
        if (world) (void)puglUpdate(world, 0.0);
        if (wake_backend) {
            wake_backend->destroy_view_on_ui_thread();
            wake_backend.reset();
        }
        if (world) puglFreeWorld(world);
    }

    [[nodiscard]] bool valid() const noexcept {
        return world && !terminal_error;
    }

    [[nodiscard]] bool runnable() const noexcept {
        return valid() && !quit_requested;
    }

    [[nodiscard]] std::vector<std::shared_ptr<detail::DispatcherOwner>>
    dispatcher_snapshot() const {
        std::vector<std::shared_ptr<detail::DispatcherOwner>> owners;
        owners.reserve(open_window_count);
        for (const auto& [id, record] : windows) {
            (void)id;
            if (record.open && record.dispatcher_owner) {
                owners.push_back(record.dispatcher_owner);
            }
        }
        return owners;
    }

    std::uint64_t register_window(
        const std::shared_ptr<detail::DispatcherOwner>& dispatcher_owner) {
        if (!runnable() || !dispatcher_owner || next_window_id == 0) return 0;
        if (!wake_backend || !wake_backend->ensure_view()) {
            terminal_error = true;
            quit_requested = true;
            last_error = wake_backend && !wake_backend->last_error().empty()
                ? std::string{wake_backend->last_error()}
                : "dispatcher wake bridge initialization failed";
            return 0;
        }

        const auto id = next_window_id++;
        windows.emplace(id, ApplicationWindowRecord{true, dispatcher_owner});
        ++open_window_count;
        return id;
    }

    void mark_window_closed(std::uint64_t id) noexcept {
        const auto found = windows.find(id);
        if (found == windows.end() || !found->second.open) return;
        found->second.open = false;
        if (found->second.dispatcher_owner) found->second.dispatcher_owner->shutdown();
        if (open_window_count) --open_window_count;
        if (open_window_count == 0 && quit_policy == QuitPolicy::OnLastWindowClosed) {
            quit_requested = true;
        }
    }

    void unregister_window(std::uint64_t id) noexcept {
        const auto found = windows.find(id);
        if (found == windows.end()) return;
        if (found->second.open) mark_window_closed(id);
        if (found->second.dispatcher_owner) found->second.dispatcher_owner->shutdown();
        windows.erase(id);
    }

    void drain_dispatchers() {
        const auto owners = dispatcher_snapshot();
        for (const auto& owner : owners) {
            if (owner) (void)owner->checkpoint();
        }
    }

    bool pump(double timeout_seconds) {
        if (!runnable()) return false;
        if (!std::isfinite(timeout_seconds)) {
            terminal_error = true;
            quit_requested = true;
            last_error = "Application poll timeout must be finite";
            return false;
        }

        const auto owners = dispatcher_snapshot();
        const double requested = timeout_seconds < 0.0 ? -1.0 : timeout_seconds;
        const double pugl_timeout = bound_dispatch_timeout(requested, owners);
        const auto status = wake_backend
            ? wake_backend->update_world(pugl_timeout)
            : puglUpdate(world, pugl_timeout);
        if (status != PUGL_SUCCESS) {
            terminal_error = true;
            quit_requested = true;
            last_error = puglStrerror(status);
            return false;
        }

        // Snapshot after native dispatch: a native close may have destroyed a
        // logical owner, while another callback may have created a new one.
        // DispatcherOwner::checkpoint itself is reentrancy-safe if a callback
        // destroys its own window or a sibling.
        drain_dispatchers();
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
    explicit Impl(std::shared_ptr<detail::DispatcherWakeBackend> wake_backend = {})
        : dispatcher_owner(
              std::make_shared<detail::DispatcherOwner>(std::move(wake_backend))) {}

    std::unique_ptr<ViewCore> core;
    Application* application{};
    std::uint64_t registration_id{};
    std::string construction_error;
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;

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
        impl_->core = std::make_unique<ViewCore>(
            ui,
            *impl_,
            PUGL_PROGRAM,
            std::move(desc),
            0,
            application.impl_->world,
            [this] { mark_application_window_closed(); });
        impl_->registration_id =
            application.impl_->register_window(impl_->dispatcher_owner);
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
    impl_->core = std::make_unique<ViewCore>(
        ui,
        *impl_,
        PUGL_PROGRAM,
        std::move(desc),
        0,
        nullptr,
        [this] {
            if (impl_ && impl_->dispatcher_owner) impl_->dispatcher_owner->shutdown();
        });
}

StandaloneWindow::~StandaloneWindow() {
    if (!impl_) return;
    if (impl_->application) {
        unregister_from_application();
    } else if (impl_->dispatcher_owner) {
        impl_->dispatcher_owner->shutdown();
    }
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
    if (!impl_->core) return false;

    const auto owner = impl_->dispatcher_owner;
    std::vector<std::shared_ptr<detail::DispatcherOwner>> owners;
    if (owner) owners.push_back(owner);
    const double requested = timeout_seconds < 0.0 ? -1.0 : timeout_seconds;
    const double pugl_timeout = bound_dispatch_timeout(requested, owners);
    const bool alive = impl_->core->poll(pugl_timeout);
    if (owner) (void)owner->checkpoint();
    return alive;
}
void StandaloneWindow::request_close() {
    if (!impl_ || !impl_->core) return;
    impl_->core->request_close();
    if (impl_->application) {
        mark_application_window_closed();
    } else if (impl_->dispatcher_owner) {
        impl_->dispatcher_owner->shutdown();
    }
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
               parent),
          dispatcher_owner(std::make_shared<detail::DispatcherOwner>()) {}

    ~Impl() override {
        if (dispatcher_owner) dispatcher_owner->shutdown();
    }

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
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
};

EmbeddedView::EmbeddedView(UI& ui, NativeParentHandle parent, Size size)
    : impl_(std::make_unique<Impl>(ui, parent, size)) {}
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
