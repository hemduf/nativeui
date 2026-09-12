#include "example_support.hpp"
#include "native_window_self_test_lock.hpp"

#include <nativeui/nativeui.hpp>

#include <optional>
#include <string>

namespace {

bool same_size(ui::Size actual, ui::Size expected) {
    return actual.w == expected.w && actual.h == expected.h;
}

bool wait_for_size(ui::Application& app,
                   const ui::StandaloneWindow& window,
                   ui::Size expected) {
    // T043 makes native Configure authoritative. Do not assume a fixed number
    // of zero-time polls is enough for the window manager to echo a resize.
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
    example::NativeWindowSelfTestLock native_test_lock;
    if (!native_test_lock.valid()) {
        return example::fail("failed to acquire native window self-test lock");
    }

    ui::UI ui{
        ui::Column{
            ui::Label{"T066 window controls"},
            ui::Label{"Deterministic standalone lifecycle self-test"}}
            .padding(16.0f)
            .gap(8.0f)};

    ui::Application app;
    app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

    ui::StandaloneWindow window{
        app,
        ui,
        ui::WindowDesc{.title = "T066 self-test",
                       .size = {120.0f, 90.0f},
                       .resizable = true,
                       .min_size = ui::Size{100.0f, 80.0f},
                       .max_size = ui::Size{240.0f, 180.0f}}};
    if (!window.valid()) return example::fail("standalone window creation failed");

    const auto initial = window.size();
    if (initial.w < 100.0f || initial.h < 80.0f ||
        initial.w > 240.0f || initial.h > 180.0f) {
        return example::fail("initial size was not clamped into constraints");
    }

    if (!window.set_title("T066 UTF-8 — fenêtre")) {
        return example::fail("UTF-8 title update failed");
    }
    if (!window.set_title("")) {
        return example::fail("empty title update failed");
    }
    if (!window.set_title("T066 self-test")) {
        return example::fail("title restore failed");
    }
    if (!window.hide() || !window.hide()) {
        return example::fail("hide was not idempotent");
    }
    if (!window.show() || !window.show()) {
        return example::fail("show was not idempotent");
    }

    if (!window.set_size({120.0f, 100.0f})) {
        return example::fail("baseline size update failed");
    }
    if (!wait_for_size(app, window, {120.0f, 100.0f})) {
        return example::fail("baseline authoritative size was not observed");
    }

    if (!window.set_min_size(ui::Size{140.0f, 90.0f})) {
        return example::fail("tightening minimum size failed");
    }
    if (!wait_for_size(app, window, {140.0f, 100.0f})) {
        return example::fail("tightened minimum did not clamp authoritative window size");
    }
    const auto tightened = window.size();

    if (!window.set_min_size(ui::Size{110.0f, 85.0f})) {
        return example::fail("in-range minimum size update failed");
    }
    pump_native_events(app);
    const auto unchanged = window.size();
    if (unchanged.w != tightened.w || unchanged.h != tightened.h) {
        return example::fail("in-range minimum update unexpectedly resized window");
    }

    if (!window.set_max_size(ui::Size{220.0f, 170.0f})) {
        return example::fail("valid maximum size update failed");
    }
    if (window.set_min_size(ui::Size{221.0f, 85.0f})) {
        return example::fail("invalid minimum/maximum pair was accepted");
    }
    if (window.set_max_size(ui::Size{100.0f, 70.0f})) {
        return example::fail("invalid maximum/minimum pair was accepted");
    }

    int veto_calls = 0;
    int closed_calls = 0;
    window.on_close_request([&] {
        ++veto_calls;
        return ui::CloseDecision::Cancel;
    });
    window.on_closed([&] { ++closed_calls; });

    // Programmatic close bypasses veto and must expose accepted close intent
    // immediately while native teardown/callback completion remains deferred.
    window.request_close();
    if (!window.should_close() || window.is_closed()) {
        return example::fail("programmatic close did not enter pending state");
    }
    if (veto_calls != 0) return example::fail("programmatic close invoked veto callback");

    (void)app.poll(0.0);
    if (!window.is_closed()) return example::fail("deferred close did not complete at checkpoint");
    if (closed_calls != 1) return example::fail("on_closed did not fire exactly once");
    if (window.show() || window.hide() || window.set_title("closed") ||
        window.set_size({150.0f, 110.0f}) ||
        window.set_min_size(ui::Size{100.0f, 80.0f}) ||
        window.set_max_size(ui::Size{220.0f, 170.0f})) {
        return example::fail("closed window still accepted mutating operations");
    }

    window.request_close();
    (void)app.poll(0.0);
    if (closed_calls != 1) return example::fail("repeated close duplicated on_closed");
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
