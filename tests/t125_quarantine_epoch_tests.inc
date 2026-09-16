#pragma once

namespace t125_quarantine_epoch {

inline void run() {
    ui::State<bool> visible{true};
    ui::State<bool> survivor_value{false};
    auto retained = std::make_shared<t125_dynamic_recovery::QuarantineState>();
    retained->throw_blur_once = false;
    test::MockPlatform platform;

    ui::UI tree{
        ui::Row{
            ui::If{visible, t125_dynamic_recovery::QuarantineProbe{retained}},
            ui::Toggle{"Survivor", survivor_value}}
            .gap(8.0f)};
    tree.resize({220.0f, 64.0f});
    tree.activate(platform);
    NUI_CHECK(retained->focus_in == 1);

    ui::EventResult nested_result = ui::EventResult::Ignored;
    retained->on_blur = [&] {
        // Reverse the structural mutation before re-entering input. The epoch
        // change makes the provisional quarantine non-authoritative, and T130
        // aborts this stale removal pass at its existing epoch checkpoint.
        visible.set(true);
        nested_result = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 20.0f, 16.0f), platform);
    };

    visible.set(false);
    tree.resize({220.0f, 64.0f});

    NUI_CHECK(visible.get());
    NUI_CHECK(nested_result == ui::EventResult::Handled);
    NUI_CHECK(retained->pointer_down == 1);
    NUI_CHECK(retained->unmounts == 0);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 20.0f, 16.0f), platform);

    // A later real removal still uses the quarantine and commits normally.
    retained->on_blur = {};
    visible.set(false);
    tree.resize({220.0f, 64.0f});
    NUI_CHECK(retained->unmounts == 1);

    tree.deactivate(platform);
}

} // namespace t125_quarantine_epoch
