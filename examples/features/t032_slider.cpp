#include "example_support.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

struct DemoState {
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{false};
    ui::State<float> gain{0.50f};
    ui::State<ui::RangeValue> band{ui::RangeValue{0.25f, 0.75f}};
};

std::string percent(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(0) << value * 100.0f << '%';
    return stream.str();
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T032 — Slider and RangeSlider"},
            ui::Row{
                ui::Toggle{"Enabled", state.enabled},
                ui::Toggle{"Read only", state.read_only}
            }.gap(12.0f),
            ui::Enabled{
                state.enabled,
                ui::ReadOnly{
                    state.read_only,
                    ui::Column{
                        ui::Slider{state.gain}
                            .range(0.0f, 1.0f)
                            .step(0.05f)
                            .formatter(percent),
                        ui::RangeSlider{state.band}
                            .range(0.0f, 1.0f)
                            .step(0.05f),
                        ui::Slider{state.gain}
                            .range(0.0f, 1.0f)
                            .orientation(ui::SliderOrientation::Vertical)
                    }.gap(16.0f)
                }
            },
            ui::Label{
                "Drag either slider. RangeSlider keeps low <= high and retains the selected "
                "thumb until release. Arrow/Home/End use the same normalization as pointer input."
            }.size(12.0f).color(ui::colors::textMuted)
        }.gap(14.0f)
    };
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    example::Platform platform;
    tree.resize({620.0f, 460.0f});
    tree.activate(platform);

    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (!(state.gain.get() > 0.50f && state.gain.get() <= 1.0f)) {
        return example::fail("Slider keyboard edit failed");
    }

    const float before_read_only = state.gain.get();
    state.read_only.set(true);
    tree.dispatch(example::key(ui::Key::Right), platform);
    if (state.gain.get() != before_read_only) {
        return example::fail("ReadOnly Slider must consume without mutation");
    }

    state.read_only.set(false);
    ui::HeadlessRenderer renderer{{620.0f, 460.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("headless Slider/RangeSlider render failed");

    const auto range = state.band.get();
    if (!std::isfinite(range.low) || !std::isfinite(range.high) || range.low > range.high) {
        return example::fail("RangeSlider invariant failed");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T032 Slider and RangeSlider", {620.0f, 460.0f});
}
