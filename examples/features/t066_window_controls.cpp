#include "example_support.hpp"

#include <nativeui/nativeui.hpp>

#include <optional>
#include <string>

namespace {

int self_test() {
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
    if (!window.hide() || !window.hide()) {
        return example::fail("hide was not idempotent");
    }
    if (!window.show() || !window.show()) {
        return example::fail("show was not idempotent");
    }

    if (!window.set_min_size(ui::Size{110.0f, 85.0f})) {
        return example::fail("valid minimum size update failed");
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
    if (window.show() || window.hide() || window.set_title("closed")) {
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
