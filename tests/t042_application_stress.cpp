#include <nativeui/nativeui.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class StressFailure final : public std::runtime_error {
public:
    StressFailure(int cycle, std::string_view transition, std::string_view message)
        : std::runtime_error(
              "fixture=standalone_supported_multi_instance cycle=" + std::to_string(cycle) +
              " transition=" + std::string(transition) + ": " + std::string(message)) {}
};

void require(bool condition, int cycle, std::string_view transition, std::string_view message) {
    if (!condition) throw StressFailure(cycle, transition, message);
}

void validate_window(
    const ui::StandaloneWindow& window,
    int cycle,
    std::string_view transition) {
    require(window.valid(), cycle, transition,
            window.last_error().empty() ? "window is invalid" : window.last_error());
    require(window.native_handle() != 0, cycle, transition, "native handle is zero");
    require(window.scale_factor() > 0.0f, cycle, transition, "invalid scale factor");
    require(window.last_error().empty(), cycle, transition, window.last_error());
}

void pump(ui::Application& application, int cycle, std::string_view transition, int iterations = 3) {
    for (int i = 0; i < iterations; ++i) {
        require(application.poll(0.0), cycle, transition,
                application.last_error().empty() ? "application stopped unexpectedly"
                                                 : application.last_error());
    }
    require(application.last_error().empty(), cycle, transition, application.last_error());
}

ui::UI make_window_ui(std::string_view label) {
    return ui::UI{ui::Column{ui::Header{std::string(label)}, ui::Spacer{120.0f}}
                      .padding(8.0f)
                      .gap(4.0f)};
}

int run_shared_application_50() {
    ui::Application application;
    require(application.valid(), -1, "construct-application", application.last_error());
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    require(application.quit_policy() == ui::QuitPolicy::ExplicitOnly,
            -1, "configure-application", "ExplicitOnly quit policy was not applied");

    for (int cycle = 0; cycle < 50; ++cycle) {
        auto ui_a = make_window_ui("NativeUI T042 shared Application A");
        auto ui_b = make_window_ui("NativeUI T042 shared Application B");
        auto a = std::make_unique<ui::StandaloneWindow>(
            application,
            ui_a,
            ui::WindowDesc{
                .title = "NativeUI T042 shared Application A",
                .size = {300.0f, 140.0f},
                .resizable = true});
        auto b = std::make_unique<ui::StandaloneWindow>(
            application,
            ui_b,
            ui::WindowDesc{
                .title = "NativeUI T042 shared Application B",
                .size = {320.0f, 150.0f},
                .resizable = true});

        validate_window(*a, cycle, "construct-a");
        validate_window(*b, cycle, "construct-b");
        require(a->native_handle() != b->native_handle(),
                cycle, "construct-a-b", "A and B unexpectedly share a native handle");
        pump(application, cycle, "poll-a-b");

        require(a->set_size({310.0f, 145.0f}), cycle, "resize-a", "A set_size failed");
        require(b->set_size({330.0f, 155.0f}), cycle, "resize-b", "B set_size failed");
        pump(application, cycle, "resize-a-b", 2);
        validate_window(*a, cycle, "resize-a");
        validate_window(*b, cycle, "resize-b");

        a->request_close();
        require(a->should_close(), cycle, "close-a", "A did not observe request_close");
        require(!b->should_close(), cycle, "close-a", "closing A closed B");
        require(!application.quit_requested(),
                cycle, "close-a", "closing A requested Application quit while B remained live");
        a.reset();

        require(!application.quit_requested(),
                cycle, "destroy-a", "destroying A requested Application quit while B remained live");
        validate_window(*b, cycle, "surviving-b");
        require(b->set_size({340.0f, 160.0f}),
                cycle, "surviving-b-resize", "surviving B set_size failed");
        pump(application, cycle, "surviving-b-poll", 3);
        validate_window(*b, cycle, "surviving-b-after-poll");

        b->request_close();
        require(b->should_close(), cycle, "close-b", "B did not observe request_close");
        b.reset();
        require(!application.quit_requested(),
                cycle, "destroy-b", "ExplicitOnly requested quit after the last window closed");
    }

    application.request_quit();
    require(application.quit_requested(), -1, "request-quit", "Application quit was not requested");
    require(!application.poll(0.0), -1, "terminal-poll", "poll succeeded after explicit quit");
    require(application.last_error().empty(), -1, "terminal-poll", application.last_error());

    std::cout << "PASS standalone_supported_multi_instance: 50 A+B cycles through one explicit "
                 "Application / PUGL_PROGRAM world\n";
    return EXIT_SUCCESS;
}

} // namespace

int main() {
    try {
        return run_shared_application_50();
    } catch (const std::exception& error) {
        std::cerr << "[nativeui t042 application stress] " << error.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "[nativeui t042 application stress] unknown exception\n";
        return EXIT_FAILURE;
    }
}
