#include <nativeui/nativeui.hpp>

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class Failure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void require(bool condition, std::string_view message) {
    if (!condition) throw Failure(std::string{message});
}

bool send_native_close(ui::NativeViewHandle handle) {
    if (!handle) return false;
#if defined(__APPLE__)
    using SendId = id (*)(id, SEL);
    using SendVoid = void (*)(id, SEL, id);
    const auto send_id = reinterpret_cast<SendId>(objc_msgSend);
    const auto send_void = reinterpret_cast<SendVoid>(objc_msgSend);
    id view = reinterpret_cast<id>(handle);
    id window = send_id(view, sel_registerName("window"));
    if (!window) return false;
    send_void(window, sel_registerName("performClose:"), nullptr);
    return true;
#elif defined(_WIN32)
    return PostMessageW(reinterpret_cast<HWND>(handle), WM_CLOSE, 0, 0) != FALSE;
#elif defined(__linux__)
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;
    const Window window = static_cast<Window>(handle);
    const Atom protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    const Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = protocols;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(delete_window);
    event.xclient.data.l[1] = CurrentTime;
    const bool sent = XSendEvent(display, window, False, NoEventMask, &event) != 0;
    XFlush(display);
    XCloseDisplay(display);
    return sent;
#else
    (void)handle;
    return false;
#endif
}

bool native_window_visible(ui::NativeViewHandle handle) {
    if (!handle) return false;
#if defined(__APPLE__)
    using SendId = id (*)(id, SEL);
    using SendBool = BOOL (*)(id, SEL);
    const auto send_id = reinterpret_cast<SendId>(objc_msgSend);
    const auto send_bool = reinterpret_cast<SendBool>(objc_msgSend);
    id view = reinterpret_cast<id>(handle);
    id window = send_id(view, sel_registerName("window"));
    return window && send_bool(window, sel_registerName("isVisible"));
#elif defined(_WIN32)
    return IsWindowVisible(reinterpret_cast<HWND>(handle)) != FALSE;
#elif defined(__linux__)
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;
    XWindowAttributes attributes{};
    const bool ok = XGetWindowAttributes(
        display, static_cast<Window>(handle), &attributes) != 0;
    XCloseDisplay(display);
    return ok && attributes.map_state == IsViewable;
#else
    (void)handle;
    return false;
#endif
}

void pump(ui::Application& app, int iterations = 4) {
    for (int i = 0; i < iterations && !app.quit_requested(); ++i) {
        (void)app.poll(0.0);
    }
}

ui::WindowDesc desc(std::string title) {
    return ui::WindowDesc{
        .title = std::move(title),
        .size = {180.0f, 120.0f},
        .resizable = true,
        .min_size = ui::Size{120.0f, 90.0f},
        .max_size = ui::Size{320.0f, 240.0f}};
}

void runtime_controls_and_hidden_registration() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 runtime controls"}};
    int closed = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 runtime")};
    require(window.valid(), "window construction failed");
    require(native_window_visible(window.native_handle()), "window was not initially visible");

    require(window.set_title("T066 UTF-8 — runtime"), "set_title failed");
    require(window.set_size({40.0f, 500.0f}), "clamped set_size failed");
    require(window.set_min_size(ui::Size{130.0f, 100.0f}), "set_min_size failed");
    require(window.set_max_size(ui::Size{300.0f, 220.0f}), "set_max_size failed");
    require(!window.set_min_size(ui::Size{301.0f, 100.0f}),
            "invalid min/max pair was accepted");

    require(window.hide() && window.hide(), "hide was not idempotent");
    pump(app, 2);
    require(!app.quit_requested(), "hidden window triggered last-window quit");
    require(window.show() && window.show(), "show was not idempotent");

    window.on_closed([&] { ++closed; });
    window.request_close();
    pump(app);
    require(window.is_closed(), "programmatic close did not complete");
    require(closed == 1, "programmatic close did not fire on_closed exactly once");
    require(app.quit_requested(), "actual last-window close did not trigger quit policy");
}

void native_close_veto_then_accept() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 native close"}};
    int requests = 0;
    int closed = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 native veto")};
    require(window.valid(), "native-veto window construction failed");

    window.on_closed([&] { ++closed; });
    window.on_close_request([&] {
        ++requests;
        return ui::CloseDecision::Cancel;
    });

    require(send_native_close(window.native_handle()), "failed to send native close request");
    pump(app, 3);
    require(requests == 1, "native close did not invoke veto callback exactly once");
    require(!window.is_closed(), "vetoed native close marked window closed");
    require(!app.quit_requested(), "vetoed native close triggered last-window quit");
    require(native_window_visible(window.native_handle()),
            "vetoed native close did not leave the native window visible");
    require(window.set_title("T066 still operational"),
            "vetoed native close left window non-operational");

    window.on_close_request([&] {
        ++requests;
        return ui::CloseDecision::Accept;
    });
    require(send_native_close(window.native_handle()), "failed to send accepted native close");
    pump(app, 4);
    require(requests == 2, "accepted native close callback count mismatch");
    require(window.is_closed(), "accepted native close did not complete");
    require(closed == 1, "accepted native close did not fire on_closed exactly once");
    require(app.quit_requested(), "accepted last-window close did not trigger quit policy");
}

void destructor_is_silent_but_updates_quit_policy() {
    ui::Application app;
    int requests = 0;
    int closed = 0;
    {
        ui::UI tree{ui::Label{"T066 destructor"}};
        auto window = std::make_unique<ui::StandaloneWindow>(app, tree, desc("T066 destructor"));
        require(window->valid(), "destructor window construction failed");
        window->on_close_request([&] {
            ++requests;
            return ui::CloseDecision::Accept;
        });
        window->on_closed([&] { ++closed; });
        window.reset();
    }
    require(requests == 0, "destructor invoked on_close_request");
    require(closed == 0, "destructor invoked on_closed");
    require(app.quit_requested(), "destructor did not update OnLastWindowClosed policy");
}

void destructor_suppresses_pending_close_callback() {
    ui::Application app;
    int closed = 0;
    {
        ui::UI tree{ui::Label{"T066 pending teardown"}};
        auto window = std::make_unique<ui::StandaloneWindow>(
            app, tree, desc("T066 pending teardown"));
        require(window->valid(), "pending-teardown window construction failed");
        window->on_closed([&] { ++closed; });
        window->request_close();
        require(window->should_close() && !window->is_closed(),
                "request_close did not enter pending state");
        window.reset();
    }
    require(closed == 0, "destructor did not suppress pending on_closed callback");
    require(app.quit_requested(), "pending-close destructor did not update quit policy");
}

void two_windows_close_independently() {
    ui::Application app;
    ui::UI tree_a{ui::Label{"T066 A"}};
    ui::UI tree_b{ui::Label{"T066 B"}};
    int closed_a = 0;
    int closed_b = 0;
    ui::StandaloneWindow a{app, tree_a, desc("T066 A")};
    ui::StandaloneWindow b{app, tree_b, desc("T066 B")};
    require(a.valid() && b.valid(), "two-window construction failed");
    a.on_closed([&] { ++closed_a; });
    b.on_closed([&] { ++closed_b; });

    a.request_close();
    pump(app, 3);
    require(a.is_closed() && closed_a == 1, "window A did not close exactly once");
    require(!b.is_closed() && closed_b == 0, "closing A mutated B");
    require(!app.quit_requested(), "closing A triggered quit while B remained open");
    require(b.set_title("T066 B survives A"), "window B was not operational after A close");

    b.request_close();
    pump(app, 3);
    require(b.is_closed() && closed_b == 1, "window B did not close exactly once");
    require(app.quit_requested(), "closing final window did not trigger quit");
}

void suite() {
    runtime_controls_and_hidden_registration();
    native_close_veto_then_accept();
    destructor_is_silent_but_updates_quit_policy();
    destructor_suppresses_pending_close_callback();
    two_windows_close_independently();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t066_window_controls_platform_tests\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t066_window_controls_platform_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
