#pragma once

/// \file
/// Common typed style primitives and Button/Checkbox/Radio style resolvers.
///
/// These backend-neutral value types own no UI/tree/native objects. Resolution
/// invokes no application callbacks and performs no retained/platform mutation.
/// Resolved font-family strings/vectors are owned copies and may allocate, so
/// style resolution is not an audio-real-time operation.

#include <nativeui/theme.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

/// Interaction branch shared by all T038 widget style resolvers. Focus,
/// selection/check state and read-only remain orthogonal flags on VisualState
/// and therefore do not erase the base interaction branch.
enum class InteractionVisualState {
    /// Enabled, neither pressed nor hovered.
    Normal,
    /// Enabled pointer-hover branch.
    Hovered,
    /// Enabled active press branch; takes precedence over hover.
    Pressed,
    /// Disabled branch; takes precedence over press and hover.
    Disabled,
};

/// Backend-neutral logical/interaction snapshot consumed by typed widget style
/// resolvers. Widgets provide the flags they support; unsupported flags remain
/// false. No instance ownership or mutable global state is stored here.
struct VisualState {
    /// Whether interaction is enabled. False selects the Disabled branch.
    bool enabled{true};
    /// Orthogonal read-only presentation state.
    bool read_only{};
    /// Pointer-hover state, considered only when enabled and not pressed.
    bool hovered{};
    /// Active press state, considered only when enabled.
    bool pressed{};
    /// Orthogonal keyboard-focus presentation state.
    bool focused{};
    /// Orthogonal selection state used by Radio-style families.
    bool selected{};
    /// Orthogonal checked state used by Checkbox-style families.
    bool checked{};

    /// Exact comparison of every state flag.
    [[nodiscard]] constexpr bool operator==(const VisualState&) const noexcept = default;
};

/// Fixed v1 interaction precedence: disabled > pressed > hovered > normal.
///
/// This constexpr helper allocates nothing, invokes no callbacks and mutates no
/// state. Focus/read-only/selected/checked remain available as orthogonal flags
/// for family-specific resolution.
[[nodiscard]] constexpr InteractionVisualState resolve_interaction_state(
    const VisualState& state) noexcept {
    if (!state.enabled) return InteractionVisualState::Disabled;
    if (state.pressed) return InteractionVisualState::Pressed;
    if (state.hovered) return InteractionVisualState::Hovered;
    return InteractionVisualState::Normal;
}

/// Optional Button presentation/measurement overrides.
///
/// Empty fields inherit the preceding layer. Float geometry/text sizes use
/// logical UI units and are stored verbatim without clamping or normalization.
struct ButtonStylePatch {
    /// Background fill color.
    std::optional<Color> fill;
    /// Outline color.
    std::optional<Color> border;
    /// Label color.
    std::optional<Color> text;
    /// Outline thickness; paint-only for current Button measurement.
    std::optional<float> border_width;
    /// Rounded-corner radius; paint-only for current Button measurement.
    std::optional<float> corner_radius;
    /// Minimum measured width.
    std::optional<float> minimum_width;
    /// Measured control height.
    std::optional<float> control_height;
    /// Per-side horizontal inset added around measured label text.
    std::optional<float> horizontal_padding;
    /// Label font size; measurement-affecting.
    std::optional<float> text_size;
    /// Label weight; measurement may depend on the selected face.
    std::optional<FontWeight> text_weight;
    /// Label slant; measurement may depend on the selected face.
    std::optional<FontSlant> text_slant;
    /// Owned preferred font-family name.
    std::optional<std::string> font_family;
    /// Owned ordered fallback-family list.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete Button style recipe. `base` is applied first; one competing
/// interaction patch is then selected by disabled > pressed > hovered > normal.
/// Read-only and focus are orthogonal and are applied afterwards. T039 can pass
/// an inherited recipe as the first resolver argument without owning traversal
/// logic here.
struct ButtonStyle {
    /// Always-applied baseline patch.
    ButtonStylePatch base;
    /// Hover interaction patch.
    ButtonStylePatch hovered;
    /// Pressed interaction patch.
    ButtonStylePatch pressed;
    /// Disabled interaction patch.
    ButtonStylePatch disabled;
    /// Orthogonal patch applied after the interaction branch.
    ButtonStylePatch read_only;
    /// Final focus patch; wins on overlapping fields.
    ButtonStylePatch focused;
};

/// Concrete owned Button values after inherited/local/state resolution.
///
/// No Theme lookup occurs during resolution. Callers normally provide
/// `default_button_style(theme)` as the populated inherited recipe. A field
/// absent from both recipes keeps this struct's default value.
struct ResolvedButtonStyle {
    /// Final background fill.
    Color fill{};
    /// Final border color.
    Color border{};
    /// Final label color.
    Color text{};
    /// Final border width in logical units.
    float border_width{};
    /// Final corner radius in logical units.
    float corner_radius{};
    /// Final minimum width in logical units.
    float minimum_width{};
    /// Final measured height in logical units.
    float control_height{};
    /// Final per-side horizontal text padding in logical units.
    float horizontal_padding{};
    /// Final label size in logical units.
    float text_size{};
    /// Final label weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Final label slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned preferred family.
    std::string font_family;
    /// Owned ordered fallback families.
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_button_style_patch(ResolvedButtonStyle& target,
                                     const ButtonStylePatch& patch) {
    if (patch.fill) target.fill = *patch.fill;
    if (patch.border) target.border = *patch.border;
    if (patch.text) target.text = *patch.text;
    if (patch.border_width) target.border_width = *patch.border_width;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
    if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_button_interaction_patch(ResolvedButtonStyle& target,
                                           const ButtonStyle& style,
                                           InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_button_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_button_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_button_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Returns a fully populated Button recipe derived from `theme`.
///
/// The result owns copied font-family data and borrows nothing from `theme`.
/// It invokes no callbacks, but copying strings/vectors may allocate. Default
/// interaction variants preserve measured geometry and change paint fields.
[[nodiscard]] inline ButtonStyle default_button_style(const Theme& theme) {
    ButtonStyle style;
    style.base.fill = theme.palette.surface;
    style.base.border = theme.palette.border;
    style.base.text = theme.palette.text;
    style.base.border_width = theme.controls.border_width;
    style.base.corner_radius = theme.radii.medium;
    style.base.minimum_width = theme.controls.minimum_width;
    style.base.control_height = theme.controls.control_height;
    style.base.horizontal_padding = theme.spacing.large;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.hovered.fill = theme.palette.control_hover;

    style.pressed.fill = theme.palette.accent;
    style.pressed.border = theme.palette.accent;
    style.pressed.text = theme.palette.background;

    style.disabled.fill = theme.palette.control_background;
    style.disabled.text = theme.palette.disabled;

    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolves one Button style without scope traversal or implicit Theme lookup.
///
/// Order: inherited.base -> local.base -> selected interaction inherited/local
/// -> read_only inherited/local -> focused inherited/local. The return value
/// owns copied strings/vectors; allocation failure propagates. No callbacks are
/// invoked, so the resolver itself has no reentrancy path.
[[nodiscard]] inline ResolvedButtonStyle resolve_button_style(
    const ButtonStyle& inherited,
    const ButtonStyle& explicit_style,
    const VisualState& state) {
    ResolvedButtonStyle resolved;
    detail::apply_button_style_patch(resolved, inherited.base);
    detail::apply_button_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_button_interaction_patch(resolved, inherited, interaction);
    detail::apply_button_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_button_style_patch(resolved, inherited.read_only);
        detail::apply_button_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_button_style_patch(resolved, inherited.focused);
        detail::apply_button_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

/// Optional Checkbox presentation/measurement overrides.
///
/// Checked is orthogonal to interaction; later layers may override it. Float
/// geometry/text sizes use logical UI units and are stored verbatim.
struct CheckboxStylePatch {
    /// Checkbox square fill.
    std::optional<Color> box_fill;
    /// Checkbox square outline.
    std::optional<Color> box_border;
    /// Checkmark stroke color.
    std::optional<Color> checkmark;
    /// Label color.
    std::optional<Color> text;
    /// Checkbox square edge length; measurement-affecting.
    std::optional<float> box_size;
    /// Checkbox corner radius; paint-only for current measurement.
    std::optional<float> box_corner_radius;
    /// Checkbox outline width; paint-only for current measurement.
    std::optional<float> box_border_width;
    /// Checkmark stroke width; paint-only for current measurement.
    std::optional<float> checkmark_width;
    /// Minimum measured control width.
    std::optional<float> minimum_width;
    /// Measured control height.
    std::optional<float> control_height;
    /// Leading inset before the checkbox square.
    std::optional<float> leading_padding;
    /// Horizontal box-to-label gap.
    std::optional<float> label_gap;
    /// Label size; measurement-affecting.
    std::optional<float> text_size;
    /// Label weight.
    std::optional<FontWeight> text_weight;
    /// Label slant.
    std::optional<FontSlant> text_slant;
    /// Owned preferred font family.
    std::optional<std::string> font_family;
    /// Owned ordered fallback families.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Checkbox recipe resolved as base -> checked -> interaction -> read-only -> focus.
struct CheckboxStyle {
    /// Always-applied baseline patch.
    CheckboxStylePatch base;
    /// Checked patch applied before interaction.
    CheckboxStylePatch checked;
    /// Hover interaction patch.
    CheckboxStylePatch hovered;
    /// Pressed interaction patch.
    CheckboxStylePatch pressed;
    /// Disabled interaction patch; can override checked fields.
    CheckboxStylePatch disabled;
    /// Orthogonal patch applied after interaction.
    CheckboxStylePatch read_only;
    /// Final focus patch.
    CheckboxStylePatch focused;
};

/// Concrete owned Checkbox style after all resolution layers.
struct ResolvedCheckboxStyle {
    /// Final box fill.
    Color box_fill{};
    /// Final box outline.
    Color box_border{};
    /// Final checkmark stroke.
    Color checkmark{};
    /// Final label color.
    Color text{};
    /// Final box edge length in logical units.
    float box_size{};
    /// Final box corner radius in logical units.
    float box_corner_radius{};
    /// Final box outline width in logical units.
    float box_border_width{};
    /// Final checkmark stroke width in logical units.
    float checkmark_width{};
    /// Final minimum width in logical units.
    float minimum_width{};
    /// Final measured height in logical units.
    float control_height{};
    /// Final leading inset in logical units.
    float leading_padding{};
    /// Final box-to-label gap in logical units.
    float label_gap{};
    /// Final label size in logical units.
    float text_size{};
    /// Final label weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Final label slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned preferred family.
    std::string font_family;
    /// Owned ordered fallback families.
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_checkbox_style_patch(ResolvedCheckboxStyle& target,
                                       const CheckboxStylePatch& patch) {
    if (patch.box_fill) target.box_fill = *patch.box_fill;
    if (patch.box_border) target.box_border = *patch.box_border;
    if (patch.checkmark) target.checkmark = *patch.checkmark;
    if (patch.text) target.text = *patch.text;
    if (patch.box_size) target.box_size = *patch.box_size;
    if (patch.box_corner_radius) target.box_corner_radius = *patch.box_corner_radius;
    if (patch.box_border_width) target.box_border_width = *patch.box_border_width;
    if (patch.checkmark_width) target.checkmark_width = *patch.checkmark_width;
    if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
    if (patch.label_gap) target.label_gap = *patch.label_gap;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_checkbox_interaction_patch(ResolvedCheckboxStyle& target,
                                             const CheckboxStyle& style,
                                             InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_checkbox_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_checkbox_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_checkbox_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Returns a fully populated Checkbox recipe derived from `theme`.
///
/// The result owns copied font-family data, invokes no callbacks and may
/// allocate while copying strings/vectors.
[[nodiscard]] inline CheckboxStyle default_checkbox_style(const Theme& theme) {
    CheckboxStyle style;
    style.base.box_fill = theme.palette.control_background;
    style.base.box_border = theme.palette.border;
    style.base.checkmark = theme.palette.background;
    style.base.text = theme.palette.text;
    style.base.box_size = 18.0f;
    style.base.box_corner_radius = theme.radii.sm;
    style.base.box_border_width = theme.controls.border_width;
    style.base.checkmark_width = 2.0f;
    style.base.minimum_width = 72.0f;
    style.base.control_height = 32.0f;
    style.base.leading_padding = 3.0f;
    style.base.label_gap = 10.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.checked.box_fill = theme.palette.accent;
    style.pressed.box_border = theme.palette.accent;
    style.disabled.box_fill = theme.palette.control_background;
    style.disabled.box_border = theme.palette.border;
    style.disabled.checkmark = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.focused.box_border = theme.palette.focus;
    style.focused.box_border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolves Checkbox style as base -> checked -> interaction -> read-only -> focus.
///
/// Inherited fields precede local fields within every layer. No Theme lookup or
/// callbacks occur. Returned strings/vectors are owned; allocation failure
/// propagates. Normally pass `default_checkbox_style(theme)` as inherited.
[[nodiscard]] inline ResolvedCheckboxStyle resolve_checkbox_style(
    const CheckboxStyle& inherited,
    const CheckboxStyle& explicit_style,
    const VisualState& state) {
    ResolvedCheckboxStyle resolved;
    detail::apply_checkbox_style_patch(resolved, inherited.base);
    detail::apply_checkbox_style_patch(resolved, explicit_style.base);

    if (state.checked) {
        detail::apply_checkbox_style_patch(resolved, inherited.checked);
        detail::apply_checkbox_style_patch(resolved, explicit_style.checked);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_checkbox_interaction_patch(resolved, inherited, interaction);
    detail::apply_checkbox_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_checkbox_style_patch(resolved, inherited.read_only);
        detail::apply_checkbox_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_checkbox_style_patch(resolved, inherited.focused);
        detail::apply_checkbox_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

/// Optional RadioButton circular presentation/measurement overrides.
///
/// Selected is applied before interaction. Float geometry/text sizes use logical
/// UI units and are stored verbatim without clamping or normalization.
struct RadioStylePatch {
    /// Outer disc/ring color.
    std::optional<Color> outer_fill;
    /// Inner disc color.
    std::optional<Color> inner_fill;
    /// Selected-mark color.
    std::optional<Color> mark_fill;
    /// Label color.
    std::optional<Color> text;
    /// Outer radius; measurement-affecting.
    std::optional<float> outer_radius;
    /// Inner radius; paint-only for current measurement.
    std::optional<float> inner_radius;
    /// Selected-mark radius; paint-only for current measurement.
    std::optional<float> mark_radius;
    /// Minimum measured control width.
    std::optional<float> minimum_width;
    /// Measured control height.
    std::optional<float> control_height;
    /// Leading inset before the circular control.
    std::optional<float> leading_padding;
    /// Horizontal circle-to-label gap.
    std::optional<float> label_gap;
    /// Label size; measurement-affecting.
    std::optional<float> text_size;
    /// Label weight.
    std::optional<FontWeight> text_weight;
    /// Label slant.
    std::optional<FontSlant> text_slant;
    /// Owned preferred font family.
    std::optional<std::string> font_family;
    /// Owned ordered fallback families.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Radio recipe resolved as base -> selected -> interaction -> read-only -> focus.
struct RadioStyle {
    /// Always-applied baseline patch.
    RadioStylePatch base;
    /// Selected patch applied before interaction.
    RadioStylePatch selected;
    /// Hover interaction patch.
    RadioStylePatch hovered;
    /// Pressed interaction patch.
    RadioStylePatch pressed;
    /// Disabled interaction patch; can override selected fields.
    RadioStylePatch disabled;
    /// Orthogonal patch applied after interaction.
    RadioStylePatch read_only;
    /// Final focus patch.
    RadioStylePatch focused;
};

/// Concrete owned RadioButton style after all resolution layers.
struct ResolvedRadioStyle {
    /// Final outer disc/ring color.
    Color outer_fill{};
    /// Final inner disc color.
    Color inner_fill{};
    /// Final selected-mark color.
    Color mark_fill{};
    /// Final label color.
    Color text{};
    /// Final outer radius in logical units.
    float outer_radius{};
    /// Final inner radius in logical units.
    float inner_radius{};
    /// Final selected-mark radius in logical units.
    float mark_radius{};
    /// Final minimum width in logical units.
    float minimum_width{};
    /// Final measured height in logical units.
    float control_height{};
    /// Final leading inset in logical units.
    float leading_padding{};
    /// Final circle-to-label gap in logical units.
    float label_gap{};
    /// Final label size in logical units.
    float text_size{};
    /// Final label weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Final label slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned preferred family.
    std::string font_family;
    /// Owned ordered fallback families.
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_radio_style_patch(ResolvedRadioStyle& target,
                                    const RadioStylePatch& patch) {
    if (patch.outer_fill) target.outer_fill = *patch.outer_fill;
    if (patch.inner_fill) target.inner_fill = *patch.inner_fill;
    if (patch.mark_fill) target.mark_fill = *patch.mark_fill;
    if (patch.text) target.text = *patch.text;
    if (patch.outer_radius) target.outer_radius = *patch.outer_radius;
    if (patch.inner_radius) target.inner_radius = *patch.inner_radius;
    if (patch.mark_radius) target.mark_radius = *patch.mark_radius;
    if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
    if (patch.label_gap) target.label_gap = *patch.label_gap;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_radio_interaction_patch(ResolvedRadioStyle& target,
                                          const RadioStyle& style,
                                          InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_radio_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_radio_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_radio_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Returns a fully populated RadioButton recipe derived from `theme`.
///
/// The result owns copied font-family data, invokes no callbacks and may
/// allocate while copying strings/vectors.
[[nodiscard]] inline RadioStyle default_radio_style(const Theme& theme) {
    RadioStyle style;
    style.base.outer_fill = theme.palette.border;
    style.base.inner_fill = theme.palette.control_background;
    style.base.mark_fill = theme.palette.accent;
    style.base.text = theme.palette.text;
    style.base.outer_radius = 9.0f;
    style.base.inner_radius = 7.0f;
    style.base.mark_radius = 4.0f;
    style.base.minimum_width = 72.0f;
    style.base.control_height = 32.0f;
    style.base.leading_padding = 3.0f;
    style.base.label_gap = 9.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.pressed.outer_fill = theme.palette.accent;
    style.disabled.outer_fill = theme.palette.border;
    style.disabled.mark_fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.focused.outer_fill = theme.palette.focus;
    return style;
}

/// Resolves Radio style as base -> selected -> interaction -> read-only -> focus.
///
/// Inherited fields precede local fields within every layer. No Theme lookup or
/// callbacks occur. Returned strings/vectors are owned and may allocate.
/// Normally pass `default_radio_style(theme)` as inherited.
[[nodiscard]] inline ResolvedRadioStyle resolve_radio_style(
    const RadioStyle& inherited,
    const RadioStyle& explicit_style,
    const VisualState& state) {
    ResolvedRadioStyle resolved;
    detail::apply_radio_style_patch(resolved, inherited.base);
    detail::apply_radio_style_patch(resolved, explicit_style.base);

    if (state.selected) {
        detail::apply_radio_style_patch(resolved, inherited.selected);
        detail::apply_radio_style_patch(resolved, explicit_style.selected);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_radio_interaction_patch(resolved, inherited, interaction);
    detail::apply_radio_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_radio_style_patch(resolved, inherited.read_only);
        detail::apply_radio_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_radio_style_patch(resolved, inherited.focused);
        detail::apply_radio_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
