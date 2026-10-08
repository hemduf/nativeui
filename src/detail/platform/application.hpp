#pragma once

// Application world ownership, window registry and quit policy.
#include "dispatcher_backend.hpp"

namespace ui {

struct Application::Impl final {
    PuglWorld* world{};
    std::shared_ptr<platform_detail::PuglDispatcherWakeBackend> wake_backend;
    QuitPolicy quit_policy{QuitPolicy::OnLastWindowClosed};
    bool quit_requested{};
    bool terminal_error{};
    std::uint64_t next_window_id{1};
    std::size_t open_window_count{};
    std::unordered_map<std::uint64_t, platform_detail::ApplicationWindowRecord> windows;
    std::string last_error;

#if defined(__EMSCRIPTEN__)
    // Emscripten exposes one main-loop slot per wasm module. Keep only the
    // identity of the Application that currently owns that slot; all window,
    // renderer, input and dispatcher state remains instance-owned.
    static Impl*& browser_main_loop_owner() noexcept {
        static Impl* owner{};
        return owner;
    }

    void stop_browser_main_loop() noexcept {
        if (browser_main_loop_owner() == this) {
            browser_main_loop_owner() = nullptr;
            emscripten_cancel_main_loop();
        }
    }

    void fail_browser_main_loop(const char* message) noexcept {
        terminal_error = true;
        quit_requested = true;
        last_error.clear();
        try {
            last_error = message ? message : "Unhandled exception in browser main loop";
        } catch (...) {
            // Error reporting must never let an allocation failure cross the
            // browser callback boundary.
        }
        stop_browser_main_loop();
    }
#endif

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
#if !defined(__EMSCRIPTEN__)
        // The browser event loop never blocks inside puglUpdate, so no native
        // wake primitive (and no extra hidden Pugl view) is needed.
        wake_backend = std::make_shared<platform_detail::PuglDispatcherWakeBackend>(world);
#endif
    }

    ~Impl() {
#if defined(__EMSCRIPTEN__)
        // A retained browser callback must never outlive its Application.
        stop_browser_main_loop();
#endif

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

    [[nodiscard]] std::vector<platform_detail::WindowControlCheckpoint>
    close_control_snapshot() const {
        std::vector<platform_detail::WindowControlCheckpoint> controls;
        controls.reserve(open_window_count);
        for (const auto& [id, record] : windows) {
            (void)id;
            if (record.open && record.close_control_context &&
                record.close_control_checkpoint) {
                controls.push_back({
                    record.lifetime,
                    record.close_control_context,
                    record.close_control_checkpoint});
            }
        }
        return controls;
    }

    std::uint64_t register_window(
        const std::shared_ptr<detail::DispatcherOwner>& dispatcher_owner,
        std::weak_ptr<int> lifetime,
        void* close_control_context,
        void (*close_control_checkpoint)(void*)) {
        if (!runnable() || !dispatcher_owner || !close_control_context ||
            !close_control_checkpoint || next_window_id == 0) {
            return 0;
        }
#if !defined(__EMSCRIPTEN__)
        if (!wake_backend || !wake_backend->ensure_view()) {
            terminal_error = true;
            quit_requested = true;
            last_error = wake_backend && !wake_backend->last_error().empty()
                ? std::string{wake_backend->last_error()}
                : "dispatcher wake bridge initialization failed";
            return 0;
        }
#endif

        const auto id = next_window_id++;
        windows.emplace(
            id,
            platform_detail::ApplicationWindowRecord{
                true,
                dispatcher_owner,
                std::move(lifetime),
                close_control_context,
                close_control_checkpoint});
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

    void drain_close_controls() {
        const auto controls = close_control_snapshot();
        for (const auto& control : controls) {
            if (!control.checkpoint || !control.context || control.lifetime.expired()) continue;
            control.checkpoint(control.context);
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
        const double pugl_timeout =
            platform_detail::bound_dispatch_timeout(requested, owners);
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
        // destroys its own window or a sibling. Lifecycle controls run only
        // after dispatcher callbacks have unwound and never depend on a free
        // user-queue slot.
        drain_dispatchers();
        drain_close_controls();
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
