#include "example_support.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

struct DemoState {
    ui::State<float> progress{0.42f};
    ui::State<float> level{0.68f};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T033 — ProgressBar and Meter"},
            ui::Label{
                "Display-only bounded values: external State drives rendering; the widgets never "
                "capture input, enter focus traversal, smooth, tick or rewrite the bound value."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::ProgressBar{state.progress}
                .formatter([](float value) {
                    return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
                }),
            ui::Row{
                ui::Button{"25%", [&state] {
                    state.progress.set(0.25f);
                    state.level.set(0.25f);
                }},
                ui::Button{"50%", [&state] {
                    state.progress.set(0.50f);
                    state.level.set(0.50f);
                }},
                ui::Button{"90%", [&state] {
                    state.progress.set(0.90f);
                    state.level.set(0.90f);
                }}
            }.gap(8.0f),
            ui::Row{
                ui::Meter{state.level},
                ui::Meter{state.level}.orientation(ui::ProgressOrientation::Vertical)
            }.gap(18.0f)
        }.gap(14.0f)
    };
}

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    example::Platform platform;
    tree.resize({620.0f, 420.0f});
    tree.activate(platform);

    // Focus begins on the first external-state control, never on ProgressBar or
    // Meter. Activating it proves the displayed value is supplied by ordinary
    // application State rather than hidden widget animation/policy.
    tree.dispatch(example::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    if (std::fabs(state.progress.get() - 0.25f) > 0.0001f ||
        std::fabs(state.level.get() - 0.25f) > 0.0001f) {
        return example::fail("external progress control failed");
    }

    // Display widgets never normalize application State merely because a value
    // is outside the visible range or non-finite.
    state.progress.set(2.0f);
    state.level.set(std::numeric_limits<float>::quiet_NaN());
    if (std::fabs(state.progress.get() - 2.0f) > 0.0001f || !std::isnan(state.level.get())) {
        return example::fail("presentation clamp must not rewrite external State");
    }

    bool invalid_range_rejected = false;
    try {
        ui::State<float> value{0.0f};
        ui::UI invalid{ui::ProgressBar{value, 1.0f, 1.0f}};
        (void)invalid;
    } catch (const std::invalid_argument&) {
        invalid_range_rejected = true;
    }
    if (!invalid_range_rejected) {
        return example::fail("invalid ProgressBar range must be rejected");
    }

    ui::HeadlessRenderer renderer{{620.0f, 420.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("headless ProgressBar/Meter render failed");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(
        tree, "NativeUI T033 ProgressBar and Meter", {620.0f, 420.0f});
}
