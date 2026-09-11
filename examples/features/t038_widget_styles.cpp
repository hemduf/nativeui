#include "example_support.hpp"

namespace {

[[nodiscard]] bool same_color(ui::Color a, ui::Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct DemoState {
    ui::State<float> value{0.62f};
};

ui::ButtonStyle demo_button_style() {
    ui::ButtonStyle style;
    style.base.fill = ui::Color{0.10f, 0.14f, 0.20f, 1.0f};
    style.hovered.fill = ui::Color{0.15f, 0.22f, 0.32f, 1.0f};
    style.pressed.fill = ui::Color{0.24f, 0.58f, 0.86f, 1.0f};
    style.focused.border = ui::Color{0.62f, 0.82f, 1.0f, 1.0f};
    return style;
}

ui::SliderStyle demo_slider_style() {
    ui::SliderStyle style;
    style.base.active = ui::Color{0.24f, 0.58f, 0.86f, 1.0f};
    style.hovered.active = ui::Color{0.38f, 0.72f, 0.98f, 1.0f};
    return style;
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Typed Widget Styles"},
        ui::Label{
            "Each widget resolves the same typed visual-state precedence while keeping style data instance-local."
        }.size(12.0f),
        ui::Button{"Hover / press / focus", [] {}}.style(demo_button_style()),
        ui::Slider{state.value}.range(0.0f, 1.0f).style(demo_slider_style())
    }.gap(14.0f)};
}

int self_test() {
    const auto theme = ui::default_theme();

    ui::ScrollbarStyle style;
    style.base.track = ui::Color{0.10f, 0.12f, 0.16f, 1.0f};
    style.base.thumb = ui::Color{0.42f, 0.58f, 0.78f, 1.0f};
    style.base.thickness = 9.0f;
    style.base.minimum_thumb = 21.0f;
    style.base.corner_radius = 4.5f;
    style.hovered.thumb = ui::Color{0.52f, 0.70f, 0.94f, 1.0f};
    style.pressed.thumb = ui::Color{0.78f, 0.88f, 1.0f, 1.0f};
    style.disabled.thumb = ui::Color{0.28f, 0.30f, 0.34f, 1.0f};

    const auto inherited = ui::default_scrollbar_style(theme);
    const auto hovered = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = true, .hovered = true});
    const auto pressed = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = true, .hovered = true, .pressed = true});
    const auto disabled = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true});

    if (!same_color(hovered.thumb, *style.hovered.thumb)) {
        return example::fail("hovered ScrollbarStyle did not resolve the hover patch");
    }
    if (!same_color(pressed.thumb, *style.pressed.thumb)) {
        return example::fail("pressed ScrollbarStyle did not win over hover");
    }
    if (!same_color(disabled.thumb, *style.disabled.thumb)) {
        return example::fail("disabled ScrollbarStyle did not win over pressed/hover");
    }
    if (hovered.thickness != 9.0f || pressed.thickness != 9.0f ||
        disabled.thickness != 9.0f || hovered.minimum_thumb != 21.0f ||
        hovered.corner_radius != 4.5f) {
        return example::fail("paint-only interaction variants changed scrollbar geometry");
    }

    DemoState state;
    auto tree = make_ui(state);
    tree.resize({640.0f, 320.0f});
    ui::HeadlessRenderer renderer{{640.0f, 320.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("typed style demo render failed");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T038 Widget Styles", {640.0f, 320.0f});
}
