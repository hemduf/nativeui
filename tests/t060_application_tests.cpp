#include <nativeui/nativeui.hpp>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[T060 application] " << stage << ": " << message << '\n';
    return 1;
}

struct WindowFixture {
    ui::State<std::string> text{"T060"};
    ui::UI ui{ui::TextInput{"T060 text", text}};
};

bool valid_window(const ui::StandaloneWindow& window, std::string_view stage) {
    if (!window.valid()) {
        (void)fail(stage, window.last_error().empty() ? "window is invalid" : window.last_error());
        return false;
    }
    if (!window.native_handle()) {
        (void)fail(stage, "native handle is zero");
        return false;
    }
    if (!window.last_error().empty()) {
        (void)fail(stage, window.last_error());
        return false;
    }
    return true;
}

int multi_window() {
    ui::Application application;
    if (!application.valid()) return fail("application", application.last_error());
    if (application.quit_policy() != ui::QuitPolicy::OnLastWindowClosed) {
        return fail("application", "default quit policy is not OnLastWindowClosed");
    }

    WindowFixture a_fixture;
    WindowFixture b_fixture;
    auto a = std::make_unique<ui::StandaloneWindow>(
        application,
        a_fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 A", .size = {300.0f, 130.0f}, .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        application,
        b_fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 B", .size = {310.0f, 140.0f}, .resizable = true});
    if (!valid_window(*a, "create-a") || !valid_window(*b, "create-b")) return 1;

    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) {
            return fail("poll-a-b", application.last_error().empty()
                                        ? "application stopped with two live windows"
                                        : application.last_error());
        }
    }

    a.reset();
    if (application.quit_requested()) {
        return fail("destroy-a", "destroying A requested quit while B remained live");
    }
    if (!b->set_size({330.0f, 150.0f})) return fail("surviving-b", "set_size failed");
    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) {
            return fail("surviving-b", application.last_error().empty()
                                           ? "application stopped while B remained live"
                                           : application.last_error());
        }
    }

    b.reset();
    if (!application.quit_requested()) {
        return fail("last-window", "last-window destruction did not request quit");
    }
    if (application.poll(0.0)) return fail("last-window", "poll succeeded after normal quit");
    if (!application.last_error().empty()) return fail("last-window", application.last_error());
    return 0;
}

int explicit_only() {
    ui::Application application;
    if (!application.valid()) return fail("explicit-only", application.last_error());
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (application.quit_policy() != ui::QuitPolicy::ExplicitOnly) {
        return fail("explicit-only", "policy did not update");
    }

    WindowFixture fixture;
    {
        ui::StandaloneWindow window{
            application,
            fixture.ui,
            ui::WindowDesc{.title = "NativeUI T060 ExplicitOnly",
                           .size = {280.0f, 120.0f},
                           .resizable = true}};
        if (!valid_window(window, "explicit-only-create")) return 1;
        window.request_close();
        if (!window.should_close()) return fail("explicit-only-close", "window did not close");
        if (application.quit_requested()) {
            return fail("explicit-only-close", "ExplicitOnly requested quit on last close");
        }
    }

    if (application.quit_requested()) {
        return fail("explicit-only-destroy", "ExplicitOnly requested quit on last unregister");
    }
    application.request_quit();
    if (!application.quit_requested()) return fail("explicit-only-quit", "quit was not requested");
    if (application.poll(0.0)) return fail("explicit-only-quit", "poll succeeded after quit");
    if (!application.last_error().empty()) return fail("explicit-only-quit", application.last_error());
    return 0;
}

int request_quit() {
    ui::Application application;
    if (!application.valid()) return fail("request-quit", application.last_error());
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    WindowFixture fixture;
    ui::StandaloneWindow window{
        application,
        fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 request_quit",
                       .size = {280.0f, 120.0f},
                       .resizable = true}};
    if (!valid_window(window, "request-quit-create")) return 1;

    application.request_quit();
    application.request_quit();
    if (!application.quit_requested()) return fail("request-quit", "quit flag was not set");
    if (window.should_close()) return fail("request-quit", "request_quit closed a live window");
    if (!window.valid()) return fail("request-quit", "request_quit invalidated a live window");
    if (application.poll(0.0)) return fail("request-quit", "poll succeeded after quit request");
    if (!application.last_error().empty()) return fail("request-quit", application.last_error());
    return 0;
}

int callback_quit() {
    ui::Application application;
    if (!application.valid()) return fail("callback-quit", application.last_error());
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

    bool callback_called = false;
    ui::UI tree{ui::Button{"Quit", [&] {
        callback_called = true;
        application.request_quit();
    }}};
    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T060 callback quit",
                       .size = {180.0f, 64.0f},
                       .resizable = true}};
    if (!valid_window(window, "callback-quit-create")) return 1;

    ui::InputEvent down{};
    down.type = ui::InputType::PointerDown;
    down.position = {20.0f, 20.0f};
    ui::InputEvent up = down;
    up.type = ui::InputType::PointerUp;
    (void)tree.dispatch(down, window);
    (void)tree.dispatch(up, window);

    if (!callback_called) return fail("callback-quit", "activation callback did not run");
    if (!application.quit_requested()) return fail("callback-quit", "callback did not request quit");
    if (window.should_close()) return fail("callback-quit", "callback quit closed the live window");
    if (!window.valid()) return fail("callback-quit", "callback quit invalidated the live window");
    if (application.poll(0.0)) return fail("callback-quit", "poll resumed after callback quit");
    if (!application.last_error().empty()) return fail("callback-quit", application.last_error());
    return 0;
}

int poll_contract() {
    ui::Application application;
    if (!application.valid()) return fail("poll-contract", application.last_error());
    WindowFixture fixture;
    ui::StandaloneWindow window{
        application,
        fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 poll",
                       .size = {280.0f, 120.0f},
                       .resizable = true}};
    if (!valid_window(window, "poll-contract-create")) return 1;

    // The first blocking poll consumes the realization/show event already
    // queued by construction. This validates the documented negative-wait
    // mapping without a timer/thread or an unbounded synthetic sleep.
    if (!application.poll(-1.0)) {
        return fail("poll-negative", application.last_error().empty()
                                         ? "blocking poll stopped unexpectedly"
                                         : application.last_error());
    }
    if (!application.poll(0.0)) {
        return fail("poll-zero", application.last_error().empty()
                                     ? "non-blocking poll stopped unexpectedly"
                                     : application.last_error());
    }
    if (!application.poll(0.001)) {
        return fail("poll-positive", application.last_error().empty()
                                         ? "bounded poll stopped unexpectedly"
                                         : application.last_error());
    }
    return 0;
}

int invalid_timeout(double value, std::string_view stage) {
    ui::Application application;
    if (!application.valid()) return fail(stage, application.last_error());
    WindowFixture fixture;
    auto window = std::make_unique<ui::StandaloneWindow>(
        application,
        fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 invalid timeout",
                       .size = {280.0f, 120.0f},
                       .resizable = true});
    if (!valid_window(*window, stage)) return 1;

    if (application.poll(value)) return fail(stage, "invalid timeout unexpectedly pumped");
    if (!application.quit_requested()) return fail(stage, "invalid timeout did not enter terminal quit");
    if (application.last_error().empty()) return fail(stage, "invalid timeout did not record an error");
    if (application.poll(0.0)) return fail(stage, "terminal application resumed after timeout error");
    window.reset();
    return 0;
}

int rejected_window() {
    ui::Application application;
    if (!application.valid()) return fail("rejected-window", application.last_error());
    application.request_quit();

    WindowFixture fixture;
    ui::StandaloneWindow window{
        application,
        fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 rejected window",
                       .size = {280.0f, 120.0f},
                       .resizable = true}};
    if (window.valid()) return fail("rejected-window", "window registered on a non-runnable Application");
    if (window.native_handle()) return fail("rejected-window", "rejected window has a native handle");
    if (window.last_error().empty()) return fail("rejected-window", "rejected window has no error");
    return 0;
}

int repeated_secondary() {
    ui::Application application;
    if (!application.valid()) return fail("repeated-secondary", application.last_error());
    WindowFixture primary_fixture;
    ui::StandaloneWindow primary{
        application,
        primary_fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 primary",
                       .size = {300.0f, 130.0f},
                       .resizable = true}};
    if (!valid_window(primary, "repeated-primary")) return 1;

    for (int iteration = 0; iteration < 25; ++iteration) {
        WindowFixture secondary_fixture;
        {
            ui::StandaloneWindow secondary{
                application,
                secondary_fixture.ui,
                ui::WindowDesc{.title = "NativeUI T060 secondary",
                               .size = {260.0f, 110.0f},
                               .resizable = true}};
            if (!valid_window(secondary, "repeated-secondary-create")) return 1;
            for (int pump = 0; pump < 2; ++pump) {
                if (!application.poll(0.0)) {
                    return fail("repeated-secondary-poll", application.last_error().empty()
                                                               ? "application stopped with primary live"
                                                               : application.last_error());
                }
            }
        }
        if (application.quit_requested()) {
            return fail("repeated-secondary-destroy", "secondary destruction requested quit");
        }
    }

    if (!primary.set_size({320.0f, 140.0f})) {
        return fail("repeated-primary-survivor", "primary set_size failed");
    }
    if (!application.poll(0.0)) {
        return fail("repeated-primary-survivor", application.last_error().empty()
                                                     ? "primary did not survive secondary cycles"
                                                     : application.last_error());
    }
    return 0;
}

[[noreturn]] void terminate_with_test_exit() noexcept {
    std::_Exit(86);
}

int lifetime_violation() {
    std::set_terminate(terminate_with_test_exit);
    auto application = std::make_unique<ui::Application>();
    if (!application->valid()) return fail("lifetime-violation", application->last_error());
    WindowFixture fixture;
    auto window = std::make_unique<ui::StandaloneWindow>(
        *application,
        fixture.ui,
        ui::WindowDesc{.title = "NativeUI T060 lifetime violation",
                       .size = {280.0f, 120.0f},
                       .resizable = true});
    if (!valid_window(*window, "lifetime-violation-create")) return 1;

    // The contract requires a deterministic programmer error rather than
    // freeing the borrowed PROGRAM world and leaving the live window dangling.
    application.reset();
    return fail("lifetime-violation", "Application destruction with a live window returned");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) return fail("arguments", "expected exactly one T060 mode");
        const std::string_view mode{argv[1]};
        if (mode == "--multi-window") return multi_window();
        if (mode == "--explicit-only") return explicit_only();
        if (mode == "--request-quit") return request_quit();
        if (mode == "--callback-quit") return callback_quit();
        if (mode == "--poll-contract") return poll_contract();
        if (mode == "--invalid-timeout-nan") {
            return invalid_timeout(std::numeric_limits<double>::quiet_NaN(), "invalid-timeout-nan");
        }
        if (mode == "--invalid-timeout-inf") {
            return invalid_timeout(std::numeric_limits<double>::infinity(), "invalid-timeout-inf");
        }
        if (mode == "--rejected-window") return rejected_window();
        if (mode == "--repeated-secondary") return repeated_secondary();
        if (mode == "--lifetime-violation") return lifetime_violation();
        return fail("arguments", "unknown T060 mode");
    } catch (const std::exception& error) {
        return fail("top-level", error.what());
    } catch (...) {
        return fail("top-level", "unknown exception");
    }
}
