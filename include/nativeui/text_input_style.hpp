#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ui {

/// Typed single-line text-input presentation and measurement overrides.
/// Empty fields inherit from the already-resolved theme/inherited recipe.
struct TextInputStylePatch {
    std::optional<Color> field_fill;
    std::optional<Color> border;
    std::optional<Color> label;
    std::optional<Color> text;
    std::optional<Color> placeholder;
    std::optional<Color> selection;
    std::optional<Color> caret;
    std::optional<Color> composition_underline;
    std::optional<float> border_width;
    std::optional<float> corner_radius;
    std::optional<float> control_width;
    std::optional<float> control_height;
    std::optional<float> field_top;
    std::optional<float> field_height;
    std::optional<float> horizontal_padding;
    std::optional<float> content_vertical_inset;
    std::optional<float> label_offset_y;
    std::optional<float> label_size;
    std::optional<float> text_size;
    std::optional<float> selection_corner_radius;
    std::optional<float> selection_vertical_inset;
    std::optional<float> caret_width;
    std::optional<float> caret_vertical_inset;
    std::optional<float> composition_underline_width;
    std::optional<float> composition_underline_inset;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct TextInputStyle {
    TextInputStylePatch base;
    TextInputStylePatch hovered;
    TextInputStylePatch pressed;
    TextInputStylePatch disabled;
    TextInputStylePatch read_only;
    TextInputStylePatch focused;
};

struct ResolvedTextInputStyle {
    Color field_fill{};
    Color border{};
    Color label{};
    Color text{};
    Color placeholder{};
    Color selection{};
    Color caret{};
    Color composition_underline{};
    float border_width{};
    float corner_radius{};
    float control_width{};
    float control_height{};
    float field_top{};
    float field_height{};
    float horizontal_padding{};
    float content_vertical_inset{};
    float label_offset_y{};
    float label_size{};
    float text_size{};
    float selection_corner_radius{};
    float selection_vertical_inset{};
    float caret_width{};
    float caret_vertical_inset{};
    float composition_underline_width{};
    float composition_underline_inset{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_text_input_style_patch(ResolvedTextInputStyle& target,
                                         const TextInputStylePatch& patch) {
    if (patch.field_fill) target.field_fill = *patch.field_fill;
    if (patch.border) target.border = *patch.border;
    if (patch.label) target.label = *patch.label;
    if (patch.text) target.text = *patch.text;
    if (patch.placeholder) target.placeholder = *patch.placeholder;
    if (patch.selection) target.selection = *patch.selection;
    if (patch.caret) target.caret = *patch.caret;
    if (patch.composition_underline) target.composition_underline = *patch.composition_underline;
    if (patch.border_width) target.border_width = *patch.border_width;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
    if (patch.control_width) target.control_width = *patch.control_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.field_top) target.field_top = *patch.field_top;
    if (patch.field_height) target.field_height = *patch.field_height;
    if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
    if (patch.content_vertical_inset) target.content_vertical_inset = *patch.content_vertical_inset;
    if (patch.label_offset_y) target.label_offset_y = *patch.label_offset_y;
    if (patch.label_size) target.label_size = *patch.label_size;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.selection_corner_radius) target.selection_corner_radius = *patch.selection_corner_radius;
    if (patch.selection_vertical_inset) target.selection_vertical_inset = *patch.selection_vertical_inset;
    if (patch.caret_width) target.caret_width = *patch.caret_width;
    if (patch.caret_vertical_inset) target.caret_vertical_inset = *patch.caret_vertical_inset;
    if (patch.composition_underline_width) target.composition_underline_width = *patch.composition_underline_width;
    if (patch.composition_underline_inset) target.composition_underline_inset = *patch.composition_underline_inset;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_text_input_interaction_patch(ResolvedTextInputStyle& target,
                                               const TextInputStyle& style,
                                               InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_text_input_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_text_input_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_text_input_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

[[nodiscard]] inline TextInputStyle default_text_input_style(const Theme& theme) {
    TextInputStyle style;
    style.base.field_fill = theme.palette.control_background;
    style.base.border = theme.palette.border;
    style.base.label = theme.palette.muted_text;
    style.base.text = theme.palette.text;
    style.base.placeholder = theme.palette.muted_text;
    style.base.selection = theme.palette.selection;
    style.base.caret = colors::caret;
    style.base.composition_underline = theme.palette.focus;
    style.base.border_width = theme.controls.border_width;
    style.base.corner_radius = theme.radii.medium;
    style.base.control_width = 420.0f;
    style.base.control_height = 82.0f;
    style.base.field_top = 24.0f;
    style.base.field_height = 46.0f;
    style.base.horizontal_padding = 12.0f;
    style.base.content_vertical_inset = 4.0f;
    style.base.label_offset_y = 10.0f;
    style.base.label_size = 12.0f;
    style.base.text_size = 15.0f;
    style.base.selection_corner_radius = 3.0f;
    style.base.selection_vertical_inset = 8.0f;
    style.base.caret_width = 1.5f;
    style.base.caret_vertical_inset = 9.0f;
    style.base.composition_underline_width = 1.5f;
    style.base.composition_underline_inset = 9.0f;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    // Keep normal/hover/pressed geometry identical. Only presentation differs.
    style.hovered.border = theme.palette.control_hover;
    style.pressed.border = theme.palette.accent;
    style.disabled.text = theme.palette.disabled;
    style.disabled.label = theme.palette.disabled;
    style.disabled.placeholder = theme.palette.disabled;
    style.disabled.caret = theme.palette.disabled;
    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

[[nodiscard]] inline ResolvedTextInputStyle resolve_text_input_style(
    const TextInputStyle& inherited,
    const TextInputStyle& explicit_style,
    const VisualState& state) {
    ResolvedTextInputStyle resolved;
    detail::apply_text_input_style_patch(resolved, inherited.base);
    detail::apply_text_input_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_text_input_interaction_patch(resolved, inherited, interaction);
    detail::apply_text_input_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_text_input_style_patch(resolved, inherited.read_only);
        detail::apply_text_input_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_text_input_style_patch(resolved, inherited.focused);
        detail::apply_text_input_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
