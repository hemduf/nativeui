#include "example_support.hpp"

#include <array>

namespace {

[[nodiscard]] bool stable_button_geometry(const ui::ResolvedButtonStyle& a,
                                          const ui::ResolvedButtonStyle& b) {
    return a.minimum_width == b.minimum_width &&
           a.control_height == b.control_height &&
           a.horizontal_padding == b.horizontal_padding &&
           a.text_size == b.text_size &&
           a.text_weight == b.text_weight &&
           a.text_slant == b.text_slant &&
           a.font_family == b.font_family &&
           a.fallback_families == b.fallback_families;
}

[[nodiscard]] bool stable_slider_geometry(const ui::ResolvedSliderStyle& a,
                                          const ui::ResolvedSliderStyle& b) noexcept {
    return a.thumb_diameter == b.thumb_diameter &&
           a.focus_ring_width == b.focus_ring_width;
}

[[nodiscard]] bool stable_text_input_geometry(const ui::ResolvedTextInputStyle& a,
                                              const ui::ResolvedTextInputStyle& b) {
    return a.control_width == b.control_width &&
           a.control_height == b.control_height &&
           a.field_top == b.field_top &&
           a.field_height == b.field_height &&
           a.horizontal_padding == b.horizontal_padding &&
           a.content_vertical_inset == b.content_vertical_inset &&
           a.label_offset_y == b.label_offset_y &&
           a.label_size == b.label_size &&
           a.text_size == b.text_size &&
           a.text_weight == b.text_weight &&
           a.text_slant == b.text_slant &&
           a.font_family == b.font_family &&
           a.fallback_families == b.fallback_families;
}

[[nodiscard]] bool stable_tabs_geometry(const ui::ResolvedTabsStyle& a,
                                        const ui::ResolvedTabsStyle& b) noexcept {
    return a.header_height == b.header_height &&
           a.panel_gap == b.panel_gap &&
           a.header_corner_radius == b.header_corner_radius &&
           a.panel_corner_radius == b.panel_corner_radius &&
           a.tab_inset == b.tab_inset &&
           a.tab_corner_radius == b.tab_corner_radius &&
           a.underline_height == b.underline_height &&
           a.underline_inset == b.underline_inset &&
           a.separator_width == b.separator_width &&
           a.separator_inset == b.separator_inset &&
           a.text_size == b.text_size;
}

int self_test() {
    const auto theme = ui::default_theme();
    const std::array states{
        ui::VisualState{.enabled = true},
        ui::VisualState{.enabled = true, .hovered = true},
        ui::VisualState{.enabled = true, .hovered = true, .pressed = true},
        ui::VisualState{.enabled = true, .focused = true},
    };

    const auto button_default = ui::default_button_style(theme);
    const auto button_normal = ui::resolve_button_style(button_default, {}, states.front());
    for (const auto& state : states) {
        const auto resolved = ui::resolve_button_style(button_default, {}, state);
        if (!stable_button_geometry(button_normal, resolved)) {
            return example::fail("default Button interaction state changed measured geometry");
        }
    }

    const auto slider_default = ui::default_slider_style(theme);
    const auto slider_normal = ui::resolve_slider_style(slider_default, {}, states.front());
    for (const auto& state : states) {
        const auto resolved = ui::resolve_slider_style(slider_default, {}, state);
        if (!stable_slider_geometry(slider_normal, resolved)) {
            return example::fail("default Slider interaction state changed measured geometry");
        }
    }

    const auto text_default = ui::default_text_input_style(theme);
    const auto text_normal = ui::resolve_text_input_style(text_default, {}, states.front());
    for (const auto& state : states) {
        const auto resolved = ui::resolve_text_input_style(text_default, {}, state);
        if (!stable_text_input_geometry(text_normal, resolved)) {
            return example::fail("default TextInput interaction state changed measured geometry");
        }
    }

    const auto tabs_default = ui::default_tabs_style(theme);
    const auto tabs_normal = ui::resolve_tabs_style(tabs_default, {}, states.front());
    for (const auto& state : states) {
        const auto resolved = ui::resolve_tabs_style(tabs_default, {}, state);
        if (!stable_tabs_geometry(tabs_normal, resolved)) {
            return example::fail("default Tabs interaction state changed measured geometry");
        }
    }

    ui::ButtonStyle compact_style;
    compact_style.base.minimum_width = 111.0f;
    compact_style.base.control_height = 31.0f;

    ui::ButtonStyle large_style;
    large_style.base.minimum_width = 167.0f;
    large_style.base.control_height = 49.0f;

    ui::UI compact{ui::Button{"A", [] {}}.style(compact_style)};
    ui::UI large{ui::Button{"A", [] {}}.style(large_style)};
    const auto compact_metrics = compact.measure();
    const auto large_metrics = large.measure();
    if (compact_metrics.preferred.w != 111.0f || compact_metrics.preferred.h != 31.0f) {
        return example::fail("first Button instance did not retain its explicit style geometry");
    }
    if (large_metrics.preferred.w != 167.0f || large_metrics.preferred.h != 49.0f) {
        return example::fail("second Button instance did not retain its explicit style geometry");
    }
    if (compact_metrics.preferred == large_metrics.preferred) {
        return example::fail("explicit Button styles leaked/shared geometry between instances");
    }

    compact.resize({220.0f, 80.0f});
    large.resize({220.0f, 80.0f});
    ui::HeadlessRenderer renderer{{220.0f, 80.0f}, 1.0f};
    if (!renderer.render(compact) || !renderer.render(large)) {
        return example::fail("styled instance-isolation render failed");
    }

    return 0;
}

ui::UI make_demo() {
    ui::ButtonStyle compact_style;
    compact_style.base.minimum_width = 111.0f;
    compact_style.base.control_height = 31.0f;
    compact_style.hovered.fill = ui::Color{0.18f, 0.28f, 0.42f, 1.0f};

    ui::ButtonStyle large_style;
    large_style.base.minimum_width = 167.0f;
    large_style.base.control_height = 49.0f;
    large_style.hovered.fill = ui::Color{0.42f, 0.22f, 0.18f, 1.0f};

    return ui::UI{ui::Column{
        ui::Header{"T038 — Style invariants"},
        ui::Label{"Default interaction states keep measured geometry stable; explicit styles remain instance-local."}.size(12.0f),
        ui::Button{"Compact", [] {}}.style(compact_style),
        ui::Button{"Large", [] {}}.style(large_style),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    auto tree = make_demo();
    return example::run_window(tree, "NativeUI T038 Style Invariants", {520.0f, 260.0f});
}
