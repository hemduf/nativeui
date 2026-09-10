#include "example_support.hpp"

#include <utility>

namespace {

struct DemoState {
    ui::State<float> value{0.55f};
};

ui::Theme make_custom_theme() {
    auto theme = ui::default_theme();
    theme.palette.background = ui::Color{0.055f, 0.065f, 0.085f, 1.0f};
    theme.palette.surface = ui::Color{0.11f, 0.16f, 0.24f, 1.0f};
    theme.palette.accent = ui::Color{0.30f, 0.72f, 0.92f, 1.0f};
    theme.typography.control_size = 15.0f;
    theme.controls.minimum_width = 118.0f;
    theme.controls.control_height = 44.0f;
    return theme;
}

ui::UI make_ui(DemoState& state, ui::Theme theme) {
    return ui::UI{
        ui::Column{
            ui::Header{"T037 — Typed Theme"},
            ui::Label{
                "Theme is an ordinary per-UI value: palette and metrics can differ between "
                "simultaneous NativeUI trees without mutable global state."
            }.size(12.0f),
            ui::Button{"Theme-owned control metrics", [] {}},
            ui::Slider{state.value}.range(0.0f, 1.0f)
        }.gap(14.0f),
        std::move(theme)};
}

int self_test() {
    DemoState first_state;
    DemoState second_state;
    const auto custom_theme = make_custom_theme();

    auto first = make_ui(first_state, custom_theme);
    auto second = make_ui(second_state, ui::default_theme());
    if (!(first.theme() == custom_theme)) return example::fail("custom Theme was not retained by UI");
    if (first.theme() == second.theme()) return example::fail("two UI instances must own independent themes");

    const auto measured = first.measure(ui::Constraints::loose({640.0f, 360.0f}));
    if (!(measured.preferred.w > 0.0f) || !(measured.preferred.h > 0.0f)) {
        return example::fail("themed UI measurement failed");
    }

    first.resize({640.0f, 360.0f});
    second.resize({640.0f, 360.0f});
    ui::HeadlessRenderer renderer{{640.0f, 360.0f}, 1.0f};
    if (!renderer.render(first)) return example::fail("custom-theme render failed");
    if (!renderer.render(second)) return example::fail("default-theme render failed");

    auto replacement = custom_theme;
    replacement.palette.accent = ui::Color{0.82f, 0.36f, 0.48f, 1.0f};
    first.set_theme(replacement);
    if (!(first.theme() == replacement)) return example::fail("live Theme replacement failed");
    if (!(second.theme() == ui::default_theme())) {
        return example::fail("Theme replacement leaked into another UI");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state, make_custom_theme());
    return example::run_window(tree, "NativeUI T037 Typed Theme", {640.0f, 360.0f});
}
