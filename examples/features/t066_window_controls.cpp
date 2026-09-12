#include "example_support.hpp"

#include <nativeui/nativeui.hpp>

#if defined(__APPLE__)
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <iostream>
#include <optional>
#include <string>

namespace {

#if defined(__APPLE__)
void crash_backtrace(int signal_number) {
    void* frames[64]{};
    const int count = ::backtrace(frames, 64);
    static constexpr char marker[] = "T066 macOS crash backtrace:\n";
    (void)::write(STDERR_FILENO, marker, sizeof(marker) - 1U);
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
    ::_exit(128 + signal_number);
}

void install_crash_backtrace() {
    (void)::signal(SIGSEGV, crash_backtrace);
    (void)::signal(SIGABRT, crash_backtrace);
}
#endif

void trace_stage(const char* stage) {
#if defined(__APPLE__)
    std::cerr << "T066 self-test: " << stage << std::endl;
#else
    (void)stage;
#endif
}

bool same_size(ui::Size actual, ui::Size expected) {
    return actual.w == expected.w && actual.h == expected.h;
}

bool wait_for_size(ui::Application& app,
                   const ui::StandaloneWindow& window,
                   ui::Size expected) {
    for (int i = 0; i < 64; ++i) {
        if (same_size(window.size(), expected)) return true;
        (void)app.poll(0.01);
    }
    return same_size(window.size(), expected);
}

void pump_native_events(ui::Application& app, int iterations = 8) {
    for (int i = 0; i < iterations; ++i) (void)app.poll(0.005);
}

int self_test() {
#if defined(__APPLE__)
    install_crash_backtrace();
#endif
    trace_stage("begin");
    ui::UI ui{
        ui::Column{
            ui::Label{"T066 window controls"},
            ui::Label{"Deterministic standalone lifecycle self-test"}}
            .padding(16.0f)
            .gap(8.0f)};

    ui::Application app;
    trace_stage("application-created");

    ui::StandaloneWindow window{
        app,
        ui,
        ui::WindowDesc{.title = "T066 self-test",
                       .size = {120.0f, 90.0f},
                       .resizable = true,
                       .min_size = ui::Size{100.0f, 80.0f},
                       .max_size = ui::Size{240.0f, 180.0f}}};
    trace_stage("window-created");
    if (!window.valid()) return example::fail("standalone window creation failed");
    trace_stage("window-valid");

    const auto initial = window.size();
    trace_stage("initial-size-read");
    if (initial.w < 100.0f || initial.h < 80.0f ||
        initial.w > 240.0f || initial.h > 180.0f) {
        return example::fail("initial size was not clamped into constraints");
    }
    trace_stage("initial-size-valid");

    if (!window.set_title("T066 UTF-8 — fenêtre")) {
        return example::fail("UTF-8 title update failed");
    }
    trace_stage("utf8-title-ok");
    if (!window.set_title("")) {
        return example::fail("empty title update failed");
    }
    trace_stage("empty-title-ok");
    if (!window.set_title("T066 self-test")) {
        return example::fail("title restore failed");
    }
    trace_stage("title-restore-ok");
    if (!window.hide()) return example::fail("first hide failed");
    trace_stage("first-hide-ok");
    if (!window.hide()) return example::fail("second hide failed");
    trace_stage("second-hide-ok");
    if (!window.show()) return example::fail("first show failed");
    trace_stage("first-show-ok");
    if (!window.show()) return example::fail("second show failed");
    trace_stage("runtime-controls-ok");

    if (!window.set_size({120.0f, 100.0f})) {
        return example::fail("baseline size update failed");
    }
    if (!wait_for_size(app, window, {120.0f, 100.0f})) {
        return example::fail("baseline authoritative size was not observed");
    }
    trace_stage("baseline-size-observed");

    if (!window.set_min_size(ui::Size{140.0f, 90.0f})) {
        return example::fail("tightening minimum size failed");
    }
    if (!wait_for_size(app, window, {140.0f, 100.0f})) {
        return example::fail("tightened minimum did not clamp authoritative window size");
    }
    const auto tightened = window.size();
    trace_stage("tightened-size-observed");

    if (!window.set_min_size(ui::Size{110.0f, 85.0f})) {
        return example::fail("in-range minimum size update failed");
    }
    pump_native_events(app);
    const auto unchanged = window.size();
    if (unchanged.w != tightened.w || unchanged.h != tightened.h) {
        return example::fail("in-range minimum update unexpectedly resized window");
    }
    trace_stage("relaxed-min-ok");

    if (!window.set_max_size(ui::Size{220.0f, 170.0f})) {
        return example::fail("valid maximum size update failed");
    }
    if (window.set_min_size(ui::Size{221.0f, 85.0f})) {
        return example::fail("invalid minimum/maximum pair was accepted");
    }
    if (window.set_max_size(ui::Size{100.0f, 70.0f})) {
        return example::fail("invalid maximum/minimum pair was accepted");
    }
    trace_stage("constraint-validation-ok");

    int veto_calls = 0;
    int closed_calls = 0;
    window.on_close_request([&] {
        ++veto_calls;
        return ui::CloseDecision::Cancel;
    });
    window.on_closed([&] { ++closed_calls; });

    window.request_close();
    trace_stage("close-requested");
    if (!window.should_close() || window.is_closed()) {
        return example::fail("programmatic close did not enter pending state");
    }
    if (veto_calls != 0) return example::fail("programmatic close invoked veto callback");

    trace_stage("before-close-poll");
    (void)app.poll(0.0);
    trace_stage("after-close-poll");
    if (!window.is_closed()) return example::fail("deferred close did not complete at checkpoint");
    if (closed_calls != 1) return example::fail("on_closed did not fire exactly once");
    if (!app.quit_requested()) {
        return example::fail("accepted last-window close did not feed Application quit policy");
    }
    if (window.show() || window.hide() || window.set_title("closed") ||
        window.set_size({150.0f, 110.0f}) ||
        window.set_min_size(ui::Size{100.0f, 80.0f}) ||
        window.set_max_size(ui::Size{220.0f, 170.0f})) {
        return example::fail("closed window still accepted mutating operations");
    }

    window.request_close();
    if (closed_calls != 1) return example::fail("repeated close duplicated on_closed");
    trace_stage("returning-success");
    return 0;
}

int interactive() {
    ui::UI ui{
        ui::Column{
            ui::Header{"T066 — WINDOW CONTROLS"},
            ui::Label{"Resize the window to observe logical min/max constraints."},
            ui::Label{"The close button uses the veto/deferred-close lifecycle."},
            ui::Label{"Runtime title/show/hide use the same StandaloneWindow instance."}}
            .padding(24.0f)
            .gap(14.0f)};

    ui::Application app;
    ui::StandaloneWindow window{
        app,
        ui,
        ui::WindowDesc{.title = "NativeUI T066 Window Controls",
                       .size = {640.0f, 320.0f},
                       .resizable = true,
                       .min_size = ui::Size{420.0f, 220.0f},
                       .max_size = ui::Size{900.0f, 560.0f}}};
    if (!window.valid()) return example::fail("standalone window creation failed");

    window.on_close_request([] { return ui::CloseDecision::Accept; });
    return app.run();
}

} // namespace

int main(int argc, char** argv) {
    return example::self_test_requested(argc, argv) ? self_test() : interactive();
}
