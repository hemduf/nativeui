#pragma once

// Pugl event-loop wake backend and dispatcher checkpoints.
#include <nativeui/nativeui.hpp>
#include <pugl/gl.h>
#include <pugl/pugl.h>
#include <nativeui/detail/dispatcher_owner.hpp>
#include "../application_platform_state.hpp"
#if defined(__APPLE__)
#  include <CoreFoundation/CFRunLoop.h>
#elif defined(_WIN32)
#  include <windows.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <poll.h>
#  undef None
#endif

namespace ui {

namespace platform_detail {

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
    std::weak_ptr<int> lifetime;
    void* close_control_context{};
    void (*close_control_checkpoint)(void*){};
};

struct WindowControlCheckpoint final {
    std::weak_ptr<int> lifetime;
    void* context{};
    void (*checkpoint)(void*){};
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

} // namespace platform_detail

} // namespace ui
