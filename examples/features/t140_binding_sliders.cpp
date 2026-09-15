#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<float> gain{0.50f};
    ui::State<ui::RangeValue> band{ui::RangeValue{0.25f, 0.75f}};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T140 — Binding-backed sliders"},
            ui::Label{
                "Slider and RangeSlider use Binding<T> internally while State<T>& stays source compatible."
            }.size(12.0f),
            ui::Slider{state.gain.binding()}
                .range(0.0f, 1.0f)
                .step(0.05f),
            ui::RangeSlider{state.band.binding()}
                .range(0.0f, 1.0f)
                .step(0.05f),
        }.gap(12.0f).padding(16.0f)};
}

int self_test() {
    DemoState left;
    DemoState right;

    int left_gain_notifications = 0;
    int left_band_notifications = 0;
    int right_gain_notifications = 0;
    int right_band_notifications = 0;
    auto left_gain = left.gain.binding();
    auto left_band = left.band.binding();
    auto right_gain = right.gain.binding();
    auto right_band = right.band.binding();
    auto left_gain_subscription = left_gain.observe(
        [&left_gain_notifications](const float&) { ++left_gain_notifications; });
    auto left_band_subscription = left_band.observe(
        [&left_band_notifications](const ui::RangeValue&) { ++left_band_notifications; });
    auto right_gain_subscription = right_gain.observe(
        [&right_gain_notifications](const float&) { ++right_gain_notifications; });
    auto right_band_subscription = right_band.observe(
        [&right_band_notifications](const ui::RangeValue&) { ++right_band_notifications; });

    auto left_tree = make_ui(left);
    auto right_tree = make_ui(right);
    example::Platform left_platform;
    example::Platform right_platform;
    left_tree.resize({620.0f, 360.0f});
    right_tree.resize({620.0f, 360.0f});
    left_tree.activate(left_platform);
    right_tree.activate(right_platform);

    // Activation focuses the first focusable control, which is the Slider.
    left_tree.dispatch(example::key(ui::Key::Right), left_platform);
    if (!(left.gain.get() > 0.50f && left.gain.get() <= 1.0f)) {
        return example::fail("Binding Slider keyboard edit failed");
    }
    if (left_gain_notifications != 1 || right_gain_notifications != 0 ||
        right.gain.get() != 0.50f) {
        return example::fail("Binding Slider notification isolation failed");
    }

    left_tree.dispatch(example::key(ui::Key::Tab), left_platform);
    left_tree.dispatch(example::key(ui::Key::Right), left_platform);
    const auto edited_band = left.band.get();
    if (!(edited_band.low > 0.25f && edited_band.low <= edited_band.high)) {
        return example::fail("Binding RangeSlider keyboard edit failed");
    }
    if (left_band_notifications != 1 || right_band_notifications != 0 ||
        !(right.band.get() == ui::RangeValue{0.25f, 0.75f})) {
        return example::fail("Binding RangeSlider notification isolation failed");
    }

    ui::HeadlessRenderer renderer{{620.0f, 360.0f}, 1.0f};
    if (!renderer.render(left_tree)) return example::fail("Binding slider render failed");
    if (!renderer.render(right_tree)) return example::fail("isolated Binding slider render failed");

    ui::State<float> legacy_gain{0.50f};
    ui::State<ui::RangeValue> legacy_band{ui::RangeValue{0.25f, 0.75f}};
    [[maybe_unused]] auto legacy_slider = ui::Slider{legacy_gain}.spec();
    [[maybe_unused]] auto legacy_range_slider = ui::RangeSlider{legacy_band}.spec();

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T140 Binding sliders", {620.0f, 360.0f});
}
