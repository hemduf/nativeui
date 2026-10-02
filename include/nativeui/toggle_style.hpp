#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ui {

/// Partial Toggle presentation, geometry, and typography override.
///
/// A disengaged optional inherits the value produced by the preceding layer; an
/// engaged optional replaces it. Numeric geometry is expressed in logical UI
/// pixels and is stored verbatim, including negative/non-finite values. Font
/// family strings and fallback lists are owned by the patch.
///
/// The value contains no UI/tree/native ownership and invokes no callbacks.
struct ToggleStylePatch {
    /// Control surface color.
    std::optional<Color> fill;
    /// Outer/control border color.
    std::optional<Color> border;
    /// Label text color.
    std::optional<Color> text;
    /// Switch track color.
    std::optional<Color> track;
    /// Switch thumb color.
    std::optional<Color> thumb;
    /// Border width in logical UI pixels.
    std::optional<float> border_width;
    /// Control corner radius in logical UI pixels.
    std::optional<float> corner_radius;
    /// Preferred control width in logical UI pixels.
    std::optional<float> control_width;
    /// Preferred control height in logical UI pixels.
    std::optional<float> control_height;
    /// Leading content inset in logical UI pixels.
    std::optional<float> leading_padding;
    /// Trailing content inset in logical UI pixels.
    std::optional<float> trailing_padding;
    /// Switch track width in logical UI pixels.
    std::optional<float> track_width;
    /// Switch track height in logical UI pixels.
    std::optional<float> track_height;
    /// Switch thumb diameter in logical UI pixels.
    std::optional<float> thumb_diameter;
    /// Label text size in logical UI pixels.
    std::optional<float> text_size;
    /// Label font weight.
    std::optional<FontWeight> text_weight;
    /// Label font slant.
    std::optional<FontSlant> text_slant;
    /// Owned preferred font-family name.
    std::optional<std::string> font_family;
    /// Owned ordered fallback font-family names.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete Toggle style recipe.
///
/// Resolution order is base -> checked -> interaction -> read_only -> focused.
/// Interaction selects one branch with disabled > pressed > hovered > normal
/// precedence. Within each layer inherited values are applied before local values.
struct ToggleStyle {
    /// Normal-state baseline.
    ToggleStylePatch base;
    /// Overrides when the toggle is checked.
    ToggleStylePatch checked;
    /// Overrides while hovered.
    ToggleStylePatch hovered;
    /// Overrides while actively pressed.
    ToggleStylePatch pressed;
    /// Overrides while disabled.
    ToggleStylePatch disabled;
    /// Orthogonal read-only overrides.
    ToggleStylePatch read_only;
    /// Orthogonal focus overrides applied last.
    ToggleStylePatch focused;
};

/// Fully resolved, owned Toggle presentation and measurement snapshot.
///
/// Strings and fallback vectors are copied into the result; it retains no borrow
/// of the input recipes or Theme. Geometry remains unvalidated at this layer.
struct ResolvedToggleStyle {
    /// Resolved control surface color.
    Color fill{};
    /// Resolved border color.
    Color border{};
    /// Resolved label text color.
    Color text{};
    /// Resolved switch track color.
    Color track{};
    /// Resolved switch thumb color.
    Color thumb{};
    /// Resolved border width in logical UI pixels.
    float border_width{};
    /// Resolved corner radius in logical UI pixels.
    float corner_radius{};
    /// Resolved preferred control width in logical UI pixels.
    float control_width{};
    /// Resolved preferred control height in logical UI pixels.
    float control_height{};
    /// Resolved leading inset in logical UI pixels.
    float leading_padding{};
    /// Resolved trailing inset in logical UI pixels.
    float trailing_padding{};
    /// Resolved switch track width in logical UI pixels.
    float track_width{};
    /// Resolved switch track height in logical UI pixels.
    float track_height{};
    /// Resolved switch thumb diameter in logical UI pixels.
    float thumb_diameter{};
    /// Resolved label text size in logical UI pixels.
    float text_size{};
    /// Resolved label font weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Resolved label font slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned resolved font-family name.
    std::string font_family;
    /// Owned ordered resolved fallback families.
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

/// Builds the default Toggle recipe from a synchronously borrowed Theme.
///
/// The returned recipe owns all string/vector data and does not retain the Theme.
/// Default geometry is expressed in logical UI pixels. Copying typography data can
/// allocate; allocation failure propagates normally. No callbacks or retained-tree
/// mutation occur, and this helper is not an audio/DSP real-time-safe contract.
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

/// Resolves inherited and component-local Toggle recipes for one visual state.
///
/// Inputs are borrowed only during the call. Resolution order is
/// base -> checked -> interaction -> read_only -> focused; local values win
/// inherited values at each layer. Thus a disabled interaction may override an
/// earlier checked value, while read-only/focused patches can override both.
///
/// The result owns its strings and vectors. Those copies may allocate and failure
/// propagates; there is no fallback recipe. The resolver performs no implicit
/// Theme lookup, callback dispatch, or retained mutation. It is not an
/// audio/DSP real-time-safe API.
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
