#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ui {

/// Typed Toggle presentation/measurement overrides. Empty fields inherit from
/// the already resolved Theme/T039/component layer. Checked state, read-only
/// and focus are orthogonal to the shared T038 interaction branch.
struct ToggleStylePatch {
    std::optional<Color> fill;
    std::optional<Color> border;
    std::optional<Color> text;
    std::optional<Color> track;
    std::optional<Color> thumb;
    std::optional<float> border_width;
    std::optional<float> corner_radius;
    std::optional<float> control_width;
    std::optional<float> control_height;
    std::optional<float> leading_padding;
    std::optional<float> trailing_padding;
    std::optional<float> track_width;
    std::optional<float> track_height;
    std::optional<float> thumb_diameter;
    std::optional<float> text_size;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct ToggleStyle {
    ToggleStylePatch base;
    ToggleStylePatch checked;
    ToggleStylePatch hovered;
    ToggleStylePatch pressed;
    ToggleStylePatch disabled;
    ToggleStylePatch read_only;
    ToggleStylePatch focused;
};

struct ResolvedToggleStyle {
    Color fill{};
    Color border{};
    Color text{};
    Color track{};
    Color thumb{};
    float border_width{};
    float corner_radius{};
    float control_width{};
    float control_height{};
    float leading_padding{};
    float trailing_padding{};
    float track_width{};
    float track_height{};
    float thumb_diameter{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_toggle_style_patch(ResolvedToggleStyle& target,
                                     const ToggleStylePatch& patch) {
    if (patch.fill) target.fill = *patch.fill;
    if (patch.border) target.border = *patch.border;
    if (patch.text) target.text = *patch.text;
    if (patch.track) target.track = *patch.track;
    if (patch.thumb) target.thumb = *patch.thumb;
    if (patch.border_width) target.border_width = *patch.border_width;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
    if (patch.control_width) target.control_width = *patch.control_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
    if (patch.trailing_padding) target.trailing_padding = *patch.trailing_padding;
    if (patch.track_width) target.track_width = *patch.track_width;
    if (patch.track_height) target.track_height = *patch.track_height;
    if (patch.thumb_diameter) target.thumb_diameter = *patch.thumb_diameter;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_toggle_interaction_patch(ResolvedToggleStyle& target,
                                           const ToggleStyle& style,
                                           InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_toggle_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_toggle_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_toggle_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// T037-backed default Toggle recipe. Geometry and normal/checked presentation
/// reproduce the existing v1 Toggle contract exactly; the extra T038 states
/// remain paint-only and do not perturb the legacy track/thumb golden.
[[nodiscard]] inline ToggleStyle default_toggle_style(const Theme& theme) {
    ToggleStyle style;
    style.base.fill = theme.palette.surface;
    style.base.border = theme.palette.border;
    style.base.text = theme.palette.text;
    style.base.track = colors::toggleOff;
    style.base.thumb = theme.palette.text;
    style.base.border_width = theme.controls.border_width;
    style.base.corner_radius = theme.radii.large;
    style.base.control_width = 210.0f;
    style.base.control_height = 54.0f;
    style.base.leading_padding = 18.0f;
    style.base.trailing_padding = 18.0f;
    style.base.track_width = 44.0f;
    style.base.track_height = 26.0f;
    style.base.thumb_diameter = 18.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.checked.track = theme.palette.accent;
    style.hovered.border = theme.palette.control_hover;
    style.pressed.border = theme.palette.accent;
    style.pressed.thumb = theme.palette.active_highlight;
    style.disabled.fill = theme.palette.control_background;
    style.disabled.border = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.read_only.border = theme.palette.track;
    style.read_only.text = theme.palette.muted_text;
    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolve Toggle style from an already composed inherited recipe, a local
/// explicit recipe and the shared T038 VisualState. Checked state is applied
/// before the competing interaction branch so Disabled still wins visually.
[[nodiscard]] inline ResolvedToggleStyle resolve_toggle_style(
    const ToggleStyle& inherited,
    const ToggleStyle& explicit_style,
    const VisualState& state) {
    ResolvedToggleStyle resolved;
    detail::apply_toggle_style_patch(resolved, inherited.base);
    detail::apply_toggle_style_patch(resolved, explicit_style.base);

    if (state.checked) {
        detail::apply_toggle_style_patch(resolved, inherited.checked);
        detail::apply_toggle_style_patch(resolved, explicit_style.checked);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_toggle_interaction_patch(resolved, inherited, interaction);
    detail::apply_toggle_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_toggle_style_patch(resolved, inherited.read_only);
        detail::apply_toggle_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_toggle_style_patch(resolved, inherited.focused);
        detail::apply_toggle_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
