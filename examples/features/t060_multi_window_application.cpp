#include "example_support.hpp"
#include "native_window_self_test_lock.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace {

std::unique_ptr<ui::UI> make_window_ui(std::string title, ui::State<bool>& enabled) {
    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{std::move(title)},
            ui::Toggle{"Enabled", enabled},
        }.padding(18.0f).gap(12.0f));
}

int run_self_test() {
#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    ui::State<bool> a_enabled{true};
    ui::State<bool> b_enabled{false};
    auto a_ui = make_window_ui("T060 A", a_enabled);
    auto b_ui = make_window_ui("T060 B", b_enabled);
    if (!a_ui || !b_ui) return example::fail("failed to construct example UIs");
    b_enabled.set(true);
    return b_enabled.get() ? 0 : example::fail("state mutation failed");
#else
    example::NativeWindowSelfTestLock native_test_lock;
    if (!native_test_lock.valid()) {
        return example::fail("failed to acquire native window self-test lock");
    }

    ui::Application application;
    if (!application.valid()) {
        return example::fail(application.last_error().empty()
                                 ? "Application initialization failed"
                                 : application.last_error());
    }

    ui::State<bool> a_enabled{true};
    ui::State<bool> b_enabled{false};
    auto a_ui = make_window_ui("T060 A", a_enabled);
    auto b_ui = make_window_ui("T060 B", b_enabled);
    auto a = std::make_unique<ui::StandaloneWindow>(
        application,
        *a_ui,
        ui::WindowDesc{.title = "NativeUI T060 - Main",
                       .size = {420.0f, 220.0f},
                       .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        application,
        *b_ui,
        ui::WindowDesc{.title = "NativeUI T060 - Secondary",
                       .size = {360.0f, 180.0f},
                       .resizable = true});
    if (!a->valid() || !b->valid()) return example::fail("failed to create both windows");

    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) {
            return example::fail(application.last_error().empty()
                                     ? "Application stopped during A+B self-test"
                                     : application.last_error());
        }
    }

    a.reset();
    if (application.quit_requested()) {
        return example::fail("destroying A requested quit while B remained live");
    }
    if (!b->set_size({380.0f, 190.0f})) return example::fail("surviving B resize failed");
    for (int i = 0; i < 4; ++i) {
        if (!application.poll(0.0)) {
            return example::fail(application.last_error().empty()
                                     ? "surviving B stopped unexpectedly"
                                     : application.last_error());
        }
    }

    b.reset();
    if (!application.quit_requested()) return example::fail("last window did not request quit");
    if (!application.last_error().empty()) return example::fail(application.last_error());
    return 0;
#endif
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail("window mode is disabled in self-test-only validation builds");
#else
    try {
        ui::Application application;
        if (!application.valid()) {
            std::cerr << "Application error: " << application.last_error() << '\n';
            return 1;
        }

        ui::State<bool> main_enabled{true};
        ui::State<bool> settings_enabled{true};
        auto main_ui = make_window_ui("Main window", main_enabled);
        auto settings_ui = make_window_ui("Settings window", settings_enabled);

        ui::StandaloneWindow main_window{
            application,
            *main_ui,
            ui::WindowDesc{.title = "NativeUI T060 - Main",
                           .size = {520.0f, 300.0f},
                           .resizable = true}};
        ui::StandaloneWindow settings_window{
            application,
            *settings_ui,
            ui::WindowDesc{.title = "NativeUI T060 - Settings",
                           .size = {420.0f, 240.0f},
                           .resizable = true}};

        if (!main_window.valid() || !settings_window.valid()) {
            std::cerr << "Window creation failed: "
                      << (main_window.valid() ? settings_window.last_error()
                                              : main_window.last_error())
                      << '\n';
            return 1;
        }

        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "T060 example failed: " << error.what() << '\n';
        return 1;
    }
#endif
}
