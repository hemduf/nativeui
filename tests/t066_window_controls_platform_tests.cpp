#include <nativeui/nativeui.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include "test_support.hpp"

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

#include <cstddef>
#include <cstdlib>
#include <functional>
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

struct LifecycleCounts {
    int mounts{};
    int unmounts{};
};

class LifecycleProbeComponent final : public ui::Component {
public:
    explicit LifecycleProbeComponent(std::shared_ptr<LifecycleCounts> counts)
        : counts_(std::move(counts)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 40.0f};
    }

    void mount(ui::MountContext&) override { ++counts_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++counts_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LifecycleCounts> counts_;
};

class LifecycleProbe {
public:
    explicit LifecycleProbe(std::shared_ptr<LifecycleCounts> counts)
        : counts_(std::move(counts)) {}

    ui::Spec spec() && {
        auto counts = std::move(counts_);
        return ui::Spec{
            [counts = std::move(counts)] {
                return std::make_unique<LifecycleProbeComponent>(counts);
            },
            {}};
    }

private:
    std::shared_ptr<LifecycleCounts> counts_;
};

struct InputCloseState {
    std::function<void()> close_from_input;
    bool input_ran{};
    int unmounts{};
};

class InputCloseProbeComponent final : public ui::Component {
public:
    explicit InputCloseProbeComponent(std::shared_ptr<InputCloseState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 60.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        state_->input_ran = true;
        state_->close_from_input();
        return ui::EventResult::Handled;
    }

    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<InputCloseState> state_;
};

class InputCloseProbe {
public:
    explicit InputCloseProbe(std::shared_ptr<InputCloseState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<InputCloseProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<InputCloseState> state_;
};

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

void fill_dispatcher_to_capacity(const ui::Dispatcher& dispatcher) {
    for (std::size_t index = 0; index < ui::kDispatcherMaxPendingTasks; ++index) {
        require(dispatcher.post([] {}),
                "dispatcher rejected work before its documented capacity");
    }
    require(!dispatcher.post([] {}), "dispatcher accepted work beyond its documented capacity");
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
    auto lifecycle = std::make_shared<LifecycleCounts>();
    ui::UI tree{LifecycleProbe{lifecycle}};
    require(lifecycle->mounts == 1 && lifecycle->unmounts == 0,
            "fixture did not mount exactly once");

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
    require(lifecycle->mounts == 1 && lifecycle->unmounts == 0,
            "show/hide remounted or unmounted UI content");

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
    if (!window.set_title("T066 still operational")) {
        throw Failure("vetoed native close left window non-operational: " +
                      std::string{window.last_error()});
    }

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

void native_close_without_request_handler_accepts_once() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 no request handler"}};
    int closed = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 no handler")};
    require(window.valid(), "no-handler window construction failed");
    window.on_closed([&] { ++closed; });

    require(send_native_close(window.native_handle()), "failed first no-handler close request");
    require(send_native_close(window.native_handle()), "failed duplicate no-handler close request");
    pump(app, 4);
    require(window.is_closed(), "no-handler native close did not complete");
    require(closed == 1, "duplicate native close fired on_closed more than once");
    require(app.quit_requested(), "no-handler last-window close did not trigger quit");
}

void programmatic_close_bypasses_veto() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 bypass veto"}};
    int requests = 0;
    int closed = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 bypass veto")};
    require(window.valid(), "bypass-veto window construction failed");
    window.on_close_request([&] {
        ++requests;
        return ui::CloseDecision::Cancel;
    });
    window.on_closed([&] { ++closed; });

    window.request_close();
    pump(app, 4);
    require(requests == 0, "programmatic close incorrectly invoked veto callback");
    require(window.is_closed() && closed == 1,
            "programmatic close did not complete exactly once");
}

void throwing_programmatic_close_post_defers_until_owner_checkpoint() {
    ui::Application app;
    app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    ui::UI tree{ui::Label{"T132 throwing programmatic close"}};
    ui::StandaloneWindow window{app, tree, desc("T132 throwing programmatic close")};
    require(window.valid(), "throwing-programmatic window construction failed");

    const auto dispatcher = window.dispatcher();
    int closed = 0;
    bool callback_ran = false;
    bool closed_inside_callback = true;
    window.on_closed([&] { ++closed; });

    require(dispatcher.post([&] {
        ui::detail::DispatcherTestAccess::fail_next_post(dispatcher);
        callback_ran = true;
        window.request_close();
        closed_inside_callback = window.is_closed();
    }), "failed to enqueue throwing-programmatic driver callback");

    (void)app.poll(0.0);
    require(callback_ran, "throwing-programmatic driver callback did not run");
    require(!closed_inside_callback,
            "exception-before-enqueue synchronously closed on active callback stack");
    require(window.is_closed(),
            "owner checkpoint did not commit throwing-programmatic close");
    require(closed == 1,
            "throwing-programmatic close did not complete exactly once");
}

void throwing_native_close_post_is_contained_and_recovers() {
    ui::Application app;
    ui::UI tree{ui::Label{"T132 throwing native close"}};
    ui::StandaloneWindow window{app, tree, desc("T132 throwing native close")};
    require(window.valid(), "throwing-native window construction failed");

    int requests = 0;
    int closed = 0;
    window.on_closed([&] { ++closed; });
    window.on_close_request([&] {
        ++requests;
        return ui::CloseDecision::Cancel;
    });

    ui::detail::DispatcherTestAccess::fail_next_post(window.dispatcher());
    require(send_native_close(window.native_handle()),
            "failed throwing native close request");
    (void)app.poll(0.0);

    require(requests == 1,
            "throwing native request did not recover Requesting exactly once");
    require(!window.is_closed(), "cancelled throwing native request closed window");
    require(!app.quit_requested(), "cancelled throwing native request triggered quit");
    require(window.set_title("T132 native throw recovered"),
            "window unusable after throwing native request post");

    window.request_close();
    pump(app, 3);
    require(window.is_closed() && closed == 1,
            "cleanup close after native throw did not complete once");
}

void throwing_accepted_close_completion_commits_once() {
    ui::Application app;
    ui::UI tree{ui::Label{"T132 throwing completion"}};
    ui::StandaloneWindow window{app, tree, desc("T132 throwing completion")};
    require(window.valid(), "throwing-completion window construction failed");

    const auto dispatcher = window.dispatcher();
    int requests = 0;
    int closed = 0;
    window.on_closed([&] { ++closed; });
    window.on_close_request([&] {
        ++requests;
        ui::detail::DispatcherTestAccess::fail_next_post(dispatcher);
        return ui::CloseDecision::Accept;
    });

    require(send_native_close(window.native_handle()),
            "failed native close for throwing completion");
    (void)app.poll(0.0);

    require(requests == 1, "throwing completion veto callback count mismatch");
    require(window.is_closed(),
            "later owner checkpoint did not commit throwing completion");
    require(closed == 1,
            "throwing completion did not fire on_closed exactly once");
    require(app.quit_requested(),
            "throwing completion last-window close did not update quit bookkeeping");
}

void saturated_dispatcher_close_defers_until_owner_checkpoint() {
    ui::Application app;
    app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    ui::UI tree{ui::Label{"T132 saturated close"}};
    ui::StandaloneWindow window{app, tree, desc("T132 saturated close")};
    require(window.valid(), "saturated-close window construction failed");

    const auto dispatcher = window.dispatcher();
    int closed = 0;
    bool callback_ran = false;
    bool closed_inside_callback = true;
    window.on_closed([&] { ++closed; });

    require(dispatcher.post([&] {
        fill_dispatcher_to_capacity(dispatcher);
        callback_ran = true;
        window.request_close();
        closed_inside_callback = window.is_closed();
    }), "failed to enqueue saturated-close driver callback");

    (void)app.poll(0.0);
    require(callback_ran, "saturated-close driver callback did not run");
    require(!closed_inside_callback,
            "Dispatcher rejection synchronously closed the window on the callback stack");
    require(window.is_closed(),
            "owner lifecycle checkpoint did not complete the rejected-post close");
    require(closed == 1, "rejected-post close did not deliver on_closed exactly once");
}

void saturated_component_input_close_defers_until_owner_checkpoint() {
    ui::Application app;
    app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    auto state = std::make_shared<InputCloseState>();
    ui::UI tree{InputCloseProbe{state}};
    ui::StandaloneWindow window{app, tree, desc("T132 saturated component input")};
    require(window.valid(), "component-input window construction failed");

    const auto dispatcher = window.dispatcher();
    int closed = 0;
    bool closed_inside_input = true;
    bool unmounted_inside_input = true;
    window.on_closed([&] { ++closed; });
    state->close_from_input = [&] {
        fill_dispatcher_to_capacity(dispatcher);
        window.request_close();
        closed_inside_input = window.is_closed();
        unmounted_inside_input = state->unmounts != 0;
    };

    test::MockPlatform platform;
    require(tree.dispatch(
                test::pointer(ui::InputType::PointerDown, 24.0f, 24.0f), platform) ==
            ui::EventResult::Handled,
            "component input driver was not handled");
    require(state->input_ran, "component input callback did not run");
    require(!closed_inside_input,
            "Dispatcher rejection synchronously closed from component input callback");
    require(!unmounted_inside_input && state->unmounts == 0,
            "Dispatcher rejection unmounted the live UI on component input stack");
    require(!window.is_closed(),
            "component-input close committed before a later owner checkpoint");

    (void)app.poll(0.0);
    require(window.is_closed(),
            "owner lifecycle checkpoint did not complete component-input close");
    require(closed == 1,
            "component-input rejected-post close did not deliver on_closed exactly once");
    require(state->unmounts == 1,
            "component-input close did not unmount the UI exactly once at checkpoint");
}

void saturated_native_close_reentrant_programmatic_wins() {
    ui::Application app;
    ui::UI tree{ui::Label{"T132 saturated native close"}};
    ui::StandaloneWindow window{app, tree, desc("T132 saturated native close")};
    require(window.valid(), "saturated-native window construction failed");

    const auto dispatcher = window.dispatcher();
    int requests = 0;
    int closed = 0;
    window.on_closed([&] { ++closed; });
    window.on_close_request([&] {
        ++requests;
        window.request_close();
        return ui::CloseDecision::Cancel;
    });

    fill_dispatcher_to_capacity(dispatcher);
    require(send_native_close(window.native_handle()),
            "failed to send saturated native close request");
    (void)app.poll(0.0);

    require(requests == 1,
            "rejected native-close post did not reach the owner lifecycle checkpoint");
    require(window.is_closed(),
            "request_close inside recovered veto did not win over returned Cancel");
    require(closed == 1,
            "recovered native close did not deliver on_closed exactly once");
    require(app.quit_requested(),
            "recovered native last-window close did not update quit bookkeeping");
}

void destroy_pending_saturated_close_is_callback_silent() {
    ui::Application app;
    int closed = 0;
    ui::UI tree{ui::Label{"T132 saturated teardown"}};
    auto window = std::make_unique<ui::StandaloneWindow>(
        app, tree, desc("T132 saturated teardown"));
    require(window->valid(), "saturated-teardown window construction failed");

    const auto dispatcher = window->dispatcher();
    window->on_closed([&] { ++closed; });
    require(dispatcher.post([&] {
        fill_dispatcher_to_capacity(dispatcher);
        window->request_close();
        require(window->should_close() && !window->is_closed(),
                "failed enqueue did not leave close pending before teardown");
        window.reset();
    }), "failed to enqueue saturated-teardown driver callback");

    (void)app.poll(0.0);
    require(!window, "saturated-teardown window survived explicit destruction");
    require(closed == 0,
            "destruction after failed close enqueue invoked on_closed");
    require(app.quit_requested(),
            "destruction after failed close enqueue did not unregister last window");
}

void saturated_close_isolated_between_windows() {
    ui::Application app;
    ui::UI tree_a{ui::Label{"T132 isolated A"}};
    ui::UI tree_b{ui::Label{"T132 isolated B"}};
    ui::StandaloneWindow a{app, tree_a, desc("T132 isolated A")};
    ui::StandaloneWindow b{app, tree_b, desc("T132 isolated B")};
    require(a.valid() && b.valid(), "saturated-isolation window construction failed");

    const auto dispatcher_a = a.dispatcher();
    int closed_a = 0;
    int closed_b = 0;
    bool a_closed_inside_callback = true;
    a.on_closed([&] { ++closed_a; });
    b.on_closed([&] { ++closed_b; });

    require(dispatcher_a.post([&] {
        fill_dispatcher_to_capacity(dispatcher_a);
        a.request_close();
        a_closed_inside_callback = a.is_closed();
    }), "failed to enqueue saturated-isolation driver callback");

    (void)app.poll(0.0);
    require(!a_closed_inside_callback,
            "saturated window A closed on its active callback stack");
    require(a.is_closed() && closed_a == 1,
            "saturated window A did not close exactly once at checkpoint");
    require(!b.is_closed() && closed_b == 0,
            "saturated close in A poisoned independent window B");
    require(!app.quit_requested(),
            "closing recovered A triggered quit while B remained open");
    require(b.set_title("T132 B survives saturated A"),
            "window B was not operational after recovered A close");

    b.request_close();
    pump(app, 3);
    require(b.is_closed() && closed_b == 1,
            "window B cleanup close did not complete exactly once");
    require(app.quit_requested(),
            "final window close after recovery did not update quit bookkeeping");
}

void reentrant_request_close_inside_veto_wins() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 reentrant self close"}};
    int requests = 0;
    int closed = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 reentrant self close")};
    require(window.valid(), "reentrant-self window construction failed");
    window.on_closed([&] { ++closed; });
    window.on_close_request([&] {
        ++requests;
        window.request_close();
        return ui::CloseDecision::Cancel;
    });

    require(send_native_close(window.native_handle()), "failed reentrant-self native close");
    pump(app, 4);
    require(requests == 1, "reentrant-self veto callback count mismatch");
    require(window.is_closed() && closed == 1,
            "request_close inside veto did not win over returned Cancel");
}

void reentrant_veto_can_close_other_window() {
    ui::Application app;
    ui::UI tree_a{ui::Label{"T066 reentrant A"}};
    ui::UI tree_b{ui::Label{"T066 reentrant B"}};
    int requests_a = 0;
    int closed_b = 0;
    ui::StandaloneWindow a{app, tree_a, desc("T066 reentrant A")};
    ui::StandaloneWindow b{app, tree_b, desc("T066 reentrant B")};
    require(a.valid() && b.valid(), "reentrant-two-window construction failed");
    b.on_closed([&] { ++closed_b; });
    a.on_close_request([&] {
        ++requests_a;
        b.request_close();
        return ui::CloseDecision::Cancel;
    });

    require(send_native_close(a.native_handle()), "failed reentrant close-other request");
    pump(app, 4);
    require(requests_a == 1, "close-other veto callback count mismatch");
    require(!a.is_closed(), "close-other veto unexpectedly closed source window");
    require(b.is_closed() && closed_b == 1,
            "close-other veto did not close target exactly once");
    require(!app.quit_requested(), "close-other veto triggered quit while source remained open");
    require(a.set_title("T066 source survives close-other"),
            "source window was not operational after close-other callback");

    a.request_close();
    pump(app, 3);
    require(a.is_closed(), "source cleanup close failed");
}

void reentrant_veto_can_request_application_quit() {
    ui::Application app;
    ui::UI tree{ui::Label{"T066 request quit"}};
    int requests = 0;
    ui::StandaloneWindow window{app, tree, desc("T066 request quit")};
    require(window.valid(), "request-quit window construction failed");
    window.on_close_request([&] {
        ++requests;
        app.request_quit();
        return ui::CloseDecision::Cancel;
    });

    require(send_native_close(window.native_handle()), "failed request-quit native close");
    (void)app.poll(0.0);
    require(requests == 1, "request-quit veto callback count mismatch");
    require(app.quit_requested(), "request_quit from veto was lost");
    require(!window.is_closed(), "request_quit plus Cancel unexpectedly closed window");
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
    native_close_without_request_handler_accepts_once();
    programmatic_close_bypasses_veto();
    throwing_programmatic_close_post_defers_until_owner_checkpoint();
    throwing_native_close_post_is_contained_and_recovers();
    throwing_accepted_close_completion_commits_once();
    saturated_dispatcher_close_defers_until_owner_checkpoint();
    saturated_component_input_close_defers_until_owner_checkpoint();
    saturated_native_close_reentrant_programmatic_wins();
    destroy_pending_saturated_close_is_callback_silent();
    saturated_close_isolated_between_windows();
    reentrant_request_close_inside_veto_wins();
    reentrant_veto_can_close_other_window();
    reentrant_veto_can_request_application_quit();
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
