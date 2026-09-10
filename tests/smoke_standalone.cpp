#include <nativeui/nativeui.hpp>

#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#if defined(__APPLE__)
#include <execinfo.h>
#include <unistd.h>
#endif

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke standalone] " << stage << ": " << message << '\n';
    return 1;
}

#if defined(NATIVEUI_ALLOW_DEPRECATED_DECLARATIONS)
void issue64_trace(std::string_view stage) {
    std::cerr << "[nativeui issue64] " << stage << '\n' << std::flush;
}

#if defined(__APPLE__)
void issue64_crash_handler(int signal) {
    static constexpr char prefix[] =
        "[nativeui issue64] fatal signal; native backtrace follows\n";
    (void)::write(STDERR_FILENO, prefix, sizeof(prefix) - 1U);

    void* frames[64]{};
    const int count = ::backtrace(frames, static_cast<int>(std::size(frames)));
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
    std::_Exit(128 + signal);
}

void install_issue64_crash_handler() {
    std::signal(SIGSEGV, issue64_crash_handler);
    std::signal(SIGABRT, issue64_crash_handler);
    std::signal(SIGBUS, issue64_crash_handler);
}
#else
void install_issue64_crash_handler() {}
#endif
#endif

bool valid_window(const ui::StandaloneWindow& window, std::string_view stage) {
    if (!window.valid()) {
        (void)fail(stage, window.last_error().empty() ? "window is invalid" : window.last_error());
        return false;
    }
    if (!window.native_handle()) {
        (void)fail(stage, "native handle is zero");
        return false;
    }
    if (!(window.scale_factor() > 0.0f)) {
        (void)fail(stage, "invalid scale factor");
        return false;
    }
    if (!window.last_error().empty()) {
        (void)fail(stage, window.last_error());
        return false;
    }
    return true;
}

int run_t060_multi_window() {
    ui::Application application;
    if (!application.valid()) {
        return fail("t060-application", application.last_error());
    }
    if (application.quit_requested()) {
        return fail("t060-application", "fresh application unexpectedly requested quit");
    }
    if (application.quit_policy() != ui::QuitPolicy::OnLastWindowClosed) {
        return fail("t060-application", "unexpected default quit policy");
    }

    ui::State<std::string> a_text{"Window A"};
    ui::State<std::string> b_text{"Window B"};
    ui::UI a_ui{ui::TextInput{"Window A text", a_text}};
    ui::UI b_ui{ui::TextInput{"Window B text", b_text}};

    auto a = std::make_unique<ui::StandaloneWindow>(
        application,
        a_ui,
        ui::WindowDesc{.title = "NativeUI T060 A", .size = {300.0f, 130.0f}, .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        application,
        b_ui,
        ui::WindowDesc{.title = "NativeUI T060 B", .size = {310.0f, 140.0f}, .resizable = true});
    if (!valid_window(*a, "t060-create-a") || !valid_window(*b, "t060-create-b")) return 1;

    a->set_text_input(true, ui::Rect{12.0f, 12.0f, 180.0f, 24.0f});
    b->set_text_input(true, ui::Rect{12.0f, 12.0f, 180.0f, 24.0f});
    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) {
            return fail("t060-poll-a-b", application.last_error().empty()
                                            ? "application stopped while both windows were live"
                                            : application.last_error());
        }
    }

    a.reset();
    if (application.quit_requested()) {
        return fail("t060-destroy-a", "destroying A requested quit while B remained live");
    }
    b->set_clipboard_text("NativeUI T060 surviving B");
    if (!b->set_size({330.0f, 150.0f})) {
        return fail("t060-surviving-b", "set_size failed");
    }
    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) {
            return fail("t060-surviving-b", application.last_error().empty()
                                             ? "application stopped while B remained live"
                                             : application.last_error());
        }
    }
    if (!b->last_error().empty()) return fail("t060-surviving-b", b->last_error());

    b.reset();
    if (!application.quit_requested()) {
        return fail("t060-last-window", "last-window destruction did not request quit");
    }
    if (application.poll(0.0)) {
        return fail("t060-last-window", "poll succeeded after normal quit request");
    }
    if (!application.last_error().empty()) {
        return fail("t060-last-window", application.last_error());
    }
    return 0;
}

#if defined(NATIVEUI_ALLOW_DEPRECATED_DECLARATIONS)
int run_issue64_sequential() {
    install_issue64_crash_handler();
    issue64_trace("sequential-begin");
    for (int iteration = 0; iteration < 3; ++iteration) {
        issue64_trace("sequential-create-" + std::to_string(iteration));
        ui::State<std::string> text{"Sequential"};
        ui::UI app{ui::TextInput{"Sequential text", text}};
        ui::StandaloneWindow window{
            app,
            ui::WindowDesc{.title = "NativeUI issue64 sequential",
                           .size = {280.0f, 120.0f},
                           .resizable = true}};
        if (!valid_window(window, "issue64-sequential-create")) return 1;

        window.set_text_input(true, ui::Rect{12.0f, 12.0f, 160.0f, 24.0f});
        window.set_clipboard_text("NativeUI issue64 sequential " + std::to_string(iteration));
        issue64_trace("sequential-poll-" + std::to_string(iteration));
        for (int i = 0; i < 4 && !window.should_close(); ++i) {
            (void)window.poll(0.0);
        }
        if (!window.last_error().empty()) {
            return fail("issue64-sequential-poll", window.last_error());
        }

        // Intentionally leave text input active. This diagnostic exercises
        // repeated destruction/recreation of independent PROGRAM worlds in one
        // process. Decision B does not make that a supported v1 ownership path;
        // T060 replaces it with one Application-owned PROGRAM world.
        issue64_trace("sequential-destroy-" + std::to_string(iteration));
    }
    issue64_trace("sequential-complete");
    return 0;
}

int run_issue64_simultaneous() {
    install_issue64_crash_handler();
    issue64_trace("simultaneous-begin");

    ui::State<std::string> a_text{"Window A"};
    ui::State<std::string> b_text{"Window B"};
    ui::UI a_ui{ui::TextInput{"Window A text", a_text}};
    ui::UI b_ui{ui::TextInput{"Window B text", b_text}};

    issue64_trace("create-a");
    auto a = std::make_unique<ui::StandaloneWindow>(
        a_ui,
        ui::WindowDesc{.title = "NativeUI issue64 A",
                       .size = {300.0f, 130.0f},
                       .resizable = true});
    if (!valid_window(*a, "issue64-create-a")) return 1;
    a->set_text_input(true, ui::Rect{12.0f, 12.0f, 180.0f, 24.0f});
    a->set_clipboard_text("NativeUI issue64 A");

    issue64_trace("create-b-while-a-live");
    auto b = std::make_unique<ui::StandaloneWindow>(
        b_ui,
        ui::WindowDesc{.title = "NativeUI issue64 B",
                       .size = {310.0f, 140.0f},
                       .resizable = true});
    if (!valid_window(*b, "issue64-create-b")) return 1;
    b->set_text_input(true, ui::Rect{12.0f, 12.0f, 180.0f, 24.0f});
    b->set_clipboard_text("NativeUI issue64 B");

    issue64_trace("poll-a-b");
    for (int i = 0; i < 8; ++i) {
        (void)a->poll(0.0);
        if (!a->last_error().empty()) return fail("issue64-poll-a", a->last_error());
        (void)b->poll(0.0);
        if (!b->last_error().empty()) return fail("issue64-poll-b", b->last_error());
    }

    // Deliberately destroy the older PROGRAM world while the newer one is
    // alive. On macOS both worlds reference the same NSApplication and each
    // PUGL_PROGRAM owns platform world-lifetime resources, so this transition
    // is the strongest ownership-order probe for issue #64.
    issue64_trace("destroy-a-while-b-live");
    a.reset();

    issue64_trace("continue-b-after-a-destruction");
    b->set_clipboard_text("NativeUI issue64 surviving B");
    if (!b->set_size({330.0f, 150.0f})) {
        return fail("issue64-surviving-b", "set_size failed");
    }
    for (int i = 0; i < 8; ++i) {
        (void)b->poll(0.0);
        if (!b->last_error().empty()) return fail("issue64-surviving-b", b->last_error());
    }

    issue64_trace("destroy-b");
    b.reset();
    issue64_trace("simultaneous-complete");
    return 0;
}
#endif

int run_regular_smoke() {
    const char* stage = "font-manager";
    try {
        ui::TextStyle font_style{};
        const auto default_face = ui::FontManager::match(font_style, U'A');
        if (!default_face || !default_face.glyph_available) {
            return fail(stage, "platform font manager could not resolve Latin text");
        }

        stage = "construct-ui";
        ui::State<bool> enabled{true};
        ui::UI app_ui{
            ui::Column{
                ui::Header{"NativeUI standalone smoke"},
                ui::Toggle{"Enabled", enabled},
            }.padding(16.0f).gap(12.0f)};

        stage = "construct-window";
        ui::Application application;
        ui::StandaloneWindow window{
            application,
            app_ui,
            ui::WindowDesc{.title = "NativeUI standalone smoke",
                           .size = {320.0f, 180.0f},
                           .resizable = true}};

        stage = "validate-native-handle";
        if (!window.native_handle()) return fail(stage, "native handle is zero");
        if (!(window.scale_factor() > 0.0f)) return fail(stage, "invalid scale factor");

        stage = "clipboard";
        window.set_clipboard_text("NativeUI clipboard smoke");

        stage = "embedded-multi-instance";
        ui::State<std::string> child_a_text{"Child A"};
        ui::State<std::string> child_b_text{"Child B"};
        ui::UI child_a_ui{ui::TextInput{"Child A text", child_a_text}};
        ui::UI child_b_ui{ui::TextInput{"Child B text", child_b_text}};
        auto child_a = std::make_unique<ui::EmbeddedView>(
            child_a_ui, window.native_handle(), ui::Size{250.0f, 100.0f});
        ui::EmbeddedView child_b{
            child_b_ui, window.native_handle(), ui::Size{250.0f, 100.0f}};

        if (!child_a->native_handle() || !child_b.native_handle()) {
            return fail(stage, "embedded native handle is zero");
        }
        child_a->set_clipboard_text("NativeUI embedded A clipboard smoke");
        child_b.set_clipboard_text("NativeUI embedded B clipboard smoke");
        for (int i = 0; i < 4; ++i) {
            (void)application.poll(0.0);
            (void)child_a->poll();
            (void)child_b.poll();
        }
        if (!child_a->last_error().empty()) return fail(stage, child_a->last_error());
        if (!child_b.last_error().empty()) return fail(stage, child_b.last_error());

        stage = "embedded-survivor";
        child_a.reset();
        child_b.set_clipboard_text("NativeUI embedded clipboard after sibling destruction");
        if (!child_b.set_size({260.0f, 110.0f})) {
            return fail(stage, "surviving embedded view set_size failed");
        }
        for (int i = 0; i < 4; ++i) {
            (void)application.poll(0.0);
            (void)child_b.poll();
        }
        if (!child_b.last_error().empty()) return fail(stage, child_b.last_error());

        stage = "embedded-repeat-create-destroy";
        for (int iteration = 0; iteration < 3; ++iteration) {
            ui::State<std::string> text{"Cycle"};
            ui::UI cycle_ui{ui::TextInput{"Cycle text", text}};
            ui::EmbeddedView cycle{
                cycle_ui, window.native_handle(), ui::Size{240.0f, 96.0f}};
            if (!cycle.native_handle()) return fail(stage, "cycle native handle is zero");
            cycle.set_clipboard_text("NativeUI embedded lifecycle " + std::to_string(iteration));
            for (int i = 0; i < 3; ++i) {
                (void)application.poll(0.0);
                (void)cycle.poll();
            }
            if (!cycle.last_error().empty()) return fail(stage, cycle.last_error());
        }

        stage = "surviving-standalone";
        window.set_clipboard_text("NativeUI clipboard after embedded lifecycle");
        if (!window.set_size({340.0f, 190.0f})) {
            return fail(stage, "surviving window set_size failed");
        }
        (void)application.poll(0.0);
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        stage = "poll";
        for (int i = 0; i < 8 && !window.should_close(); ++i) {
            if (!application.poll(0.0) && !window.should_close()) {
                return fail(stage, application.last_error().empty()
                                       ? window.last_error()
                                       : application.last_error());
            }
        }
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        stage = "resize";
        if (!window.set_size({360.0f, 200.0f})) return fail(stage, "set_size failed");
        for (int i = 0; i < 4 && !window.should_close(); ++i) {
            (void)application.poll(0.0);
        }
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        stage = "close";
        window.request_close();
        if (!window.should_close()) return fail(stage, "request_close did not update state");
        return 0;
    } catch (const std::exception& e) {
        return fail(stage, e.what());
    } catch (...) {
        return fail(stage, "unknown exception");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const std::string_view mode{argv[1]};
            if (mode == "--t060-multi-window") return run_t060_multi_window();
#if defined(NATIVEUI_ALLOW_DEPRECATED_DECLARATIONS)
            if (mode == "--issue64-sequential") return run_issue64_sequential();
            if (mode == "--issue64-simultaneous") return run_issue64_simultaneous();
#else
            if (mode == "--issue64-sequential" || mode == "--issue64-simultaneous") {
                return fail(
                    "arguments",
                    "legacy issue64 diagnostics require -DNATIVEUI_ALLOWED_WARNINGS=deprecated-declarations");
            }
#endif
            return fail("arguments", "unknown diagnostic mode");
        }
        if (argc != 1) return fail("arguments", "expected at most one diagnostic mode");

        return run_regular_smoke();
    } catch (const std::exception& e) {
        return fail("top-level", e.what());
    } catch (...) {
        return fail("top-level", "unknown exception");
    }
}
