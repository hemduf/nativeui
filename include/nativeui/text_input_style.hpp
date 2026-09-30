#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

/// \file
/// Single-line TextInput style values. Float geometry uses logical UI units.
/// Patches are stored verbatim; resolving may allocate for owned font strings.
namespace ui {

/// Typed single-line TextInput presentation and measurement overrides.
///
/// A disengaged optional inherits the value already produced by an earlier
/// recipe layer; an engaged optional replaces it verbatim. Numeric geometry is
/// expressed in logical UI units and is deliberately not clamped or normalized
/// here, so callers can detect/configure invalid values before a live widget
/// consumes the resolved recipe.
///
/// Geometry participates in different retained invalidation classes:
/// control/field extents, padding, label/text sizes and font selection can affect
/// measurement/layout; border, selection, caret and composition geometry are
/// paint geometry inside the published field bounds. The patch itself does not
/// classify or publish invalidation.
///
/// Font-family strings and fallback lists are owned. The patch retains no Theme,
/// UI, Tree, widget, renderer, or native handle, invokes no callbacks, and has no
/// reentrant dispatch path. Copying owned strings/vectors may allocate, so style
/// construction/resolution is UI/setup work rather than an audio-RT contract.
struct TextInputStylePatch {
    /// Field background.
    std::optional<Color> field_fill;
    /// Field border color.
    std::optional<Color> border;
    /// Label color.
    std::optional<Color> label;
    /// Editor/preedit text color.
    std::optional<Color> text;
    /// Placeholder color.
    std::optional<Color> placeholder;
    /// Selection highlight color.
    std::optional<Color> selection;
    /// Caret color.
    std::optional<Color> caret;
    /// IME/preedit underline color.
    std::optional<Color> composition_underline;
    /// Border width in logical units.
    std::optional<float> border_width;
    /// Field corner radius in logical units.
    std::optional<float> corner_radius;
    /// Preferred measured control width.
    std::optional<float> control_width;
    /// Preferred measured control height.
    std::optional<float> control_height;
    /// Field top offset from control top.
    std::optional<float> field_top;
    /// Field height.
    std::optional<float> field_height;
    /// Left/right editable-content inset.
    std::optional<float> horizontal_padding;
    /// Top/bottom editable-content inset.
    std::optional<float> content_vertical_inset;
    /// Label center-Y offset from control top.
    std::optional<float> label_offset_y;
    /// Label font size.
    std::optional<float> label_size;
    /// Editor font size.
    std::optional<float> text_size;
    /// Selection highlight radius.
    std::optional<float> selection_corner_radius;
    /// Selection vertical inset.
    std::optional<float> selection_vertical_inset;
    /// Caret stroke width.
    std::optional<float> caret_width;
    /// Caret top/bottom inset.
    std::optional<float> caret_vertical_inset;
    /// IME underline stroke width.
    std::optional<float> composition_underline_width;
    /// IME underline distance from field bottom.
    std::optional<float> composition_underline_inset;
    /// Label/editor font weight.
    std::optional<FontWeight> text_weight;
    /// Label/editor font slant.
    std::optional<FontSlant> text_slant;
    /// Preferred font family.
    std::optional<std::string> font_family;
    /// Ordered fallback font families.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete single-line TextInput recipe.
///
/// Resolution order is base -> one interaction branch -> read_only -> focused.
/// Interaction precedence is disabled > pressed > hovered > normal. Within every
/// layer inherited fields are applied before component-local fields, so the
/// component-local recipe wins on overlap.
///
/// Recipes are detached owned values. They may be copied, retained, or built
/// before a UI exists and borrow nothing from the Theme used to create defaults.
/// Merely mutating a recipe does not touch retained state or invoke invalidation;
/// that happens only when a live TextInput consumes a new resolved style.
struct TextInputStyle {
    /// Base patch.
    TextInputStylePatch base;
    /// Hover interaction patch.
    TextInputStylePatch hovered;
    /// Pressed interaction patch.
    TextInputStylePatch pressed;
    /// Disabled interaction patch.
    TextInputStylePatch disabled;
    /// Read-only orthogonal patch.
    TextInputStylePatch read_only;
    /// Focused orthogonal patch.
    TextInputStylePatch focused;
};

/// Concrete single-line editor presentation/measurement values after resolution.
///
/// The snapshot is fully owned: font strings/vectors are copied and no field
/// borrows from either input recipe or Theme. Default construction performs no
/// Theme lookup, so a field omitted by both inherited and local recipes remains
/// its ordinary zero/default value.
///
/// Numeric fields preserve the exact selected recipe payload, including
/// negative/non-finite values. Resolution therefore never hides invalid style
/// data behind fallback/clamping; validation/canonicalization belongs to the
/// consuming widget/renderer contract.
struct ResolvedTextInputStyle {
    /// Resolved field background.
    Color field_fill{};
    /// Resolved border color.
    Color border{};
    /// Resolved label color.
    Color label{};
    /// Resolved text color.
    Color text{};
    /// Resolved placeholder color.
    Color placeholder{};
    /// Resolved selection color.
    Color selection{};
    /// Resolved caret color.
    Color caret{};
    /// Resolved IME underline color.
    Color composition_underline{};
    /// Resolved border width, logical units.
    float border_width{};
    /// Resolved corner radius, logical units.
    float corner_radius{};
    /// Resolved preferred width, logical units.
    float control_width{};
    /// Resolved preferred height, logical units.
    float control_height{};
    /// Resolved field top offset, logical units.
    float field_top{};
    /// Resolved field height, logical units.
    float field_height{};
    /// Resolved horizontal content inset.
    float horizontal_padding{};
    /// Resolved vertical content inset.
    float content_vertical_inset{};
    /// Resolved label center-Y offset.
    float label_offset_y{};
    /// Resolved label font size.
    float label_size{};
    /// Resolved editor font size.
    float text_size{};
    /// Resolved selection radius.
    float selection_corner_radius{};
    /// Resolved selection vertical inset.
    float selection_vertical_inset{};
    /// Resolved caret width.
    float caret_width{};
    /// Resolved caret inset.
    float caret_vertical_inset{};
    /// Resolved IME underline width.
    float composition_underline_width{};
    /// Resolved IME underline inset.
    float composition_underline_inset{};
    /// Resolved font weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Resolved font slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned resolved preferred family.
    std::string font_family;
    /// Owned resolved fallback families.
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

/// Build the complete default TextInput recipe from a synchronously borrowed Theme.
///
/// The Theme is read only for this call. The result owns copied family/fallback
/// strings and can outlive the Theme. Default interaction variants preserve the
/// normal control/field geometry; hover/pressed/disabled/focused defaults change
/// presentation only.
///
/// No callback, retained-tree mutation, or platform/native lookup occurs here.
/// Copying Theme-owned strings/vectors may allocate and allocation failure
/// propagates. This helper is intended for UI/style setup, not audio/DSP RT use.
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

/// Resolve inherited and component-local TextInput recipes for one VisualState.
///
/// Inputs are borrowed only for the synchronous call. Resolution is
/// base -> interaction -> read_only -> focused, with
/// disabled > pressed > hovered > normal interaction precedence and local fields
/// overriding inherited fields within each layer. The returned snapshot owns all
/// copied font strings/vectors.
///
/// The resolver performs no Theme lookup, numeric sanitization, retained-state
/// mutation, invalidation, callback invocation, or reentrant UI dispatch. Fields
/// omitted by both recipes remain default constructed. Owned string/vector copies
/// may allocate and allocation failure propagates.
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
