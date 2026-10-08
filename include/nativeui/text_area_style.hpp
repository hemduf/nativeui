#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

/// \file
/// Multiline TextArea style values; float geometry uses logical UI units.
/// Patch values are stored verbatim; resolution may allocate for font data.
namespace ui {

/// Typed multiline TextArea presentation and measurement overrides.
///
/// A disengaged optional inherits the value already selected by an earlier
/// recipe layer; an engaged optional replaces it verbatim. All scalar geometry
/// is in logical UI units and is intentionally stored without clamping or
/// finiteness checks.
///
/// Control/field extents, padding, line height, label/text sizes and font
/// selection can affect measurement/layout. Border, selection, caret, newline
/// selection extension and composition-underline geometry are paint geometry
/// inside those retained bounds. This value layer does not itself publish or
/// classify invalidation.
///
/// Font-family strings and fallback lists are owned. The patch borrows no Theme,
/// UI, Tree, widget, renderer, or native object and invokes no callbacks. Copies
/// may allocate for owned strings/vectors, so style preparation is not an
/// audio/DSP real-time API.
struct TextAreaStylePatch {
    /// Editor field background color.
    std::optional<Color> field_fill;
    /// Field border color.
    std::optional<Color> border;
    /// Label color.
    std::optional<Color> label;
    /// Body and preedit text color.
    std::optional<Color> text;
    /// Placeholder color.
    std::optional<Color> placeholder;
    /// Selection highlight color.
    std::optional<Color> selection;
    /// Caret color.
    std::optional<Color> caret;
    /// IME/preedit underline color.
    std::optional<Color> composition_underline;
    /// Border stroke width in logical UI units.
    std::optional<float> border_width;
    /// Field corner radius in logical UI units.
    std::optional<float> corner_radius;
    /// Preferred measured control width in logical UI units.
    std::optional<float> control_width;
    /// Preferred measured control height in logical UI units.
    std::optional<float> control_height;
    /// Field top offset from control top in logical UI units.
    std::optional<float> field_top;
    /// Minimum field height in logical UI units.
    std::optional<float> minimum_field_height;
    /// Left/right content inset in logical UI units.
    std::optional<float> horizontal_padding;
    /// Top/bottom content inset in logical UI units.
    std::optional<float> vertical_padding;
    /// Label center-Y offset from control top.
    std::optional<float> label_offset_y;
    /// Label font size in logical UI units.
    std::optional<float> label_size;
    /// Body font size in logical UI units.
    std::optional<float> text_size;
    /// Vertical advance per logical text line.
    std::optional<float> line_height;
    /// Selection highlight radius.
    std::optional<float> selection_corner_radius;
    /// Selection vertical inset within each line.
    std::optional<float> selection_vertical_inset;
    /// Extra highlight width when selection includes a newline.
    std::optional<float> newline_selection_width;
    /// Caret stroke width.
    std::optional<float> caret_width;
    /// Caret top/bottom inset within a line.
    std::optional<float> caret_vertical_inset;
    /// IME underline stroke width.
    std::optional<float> composition_underline_width;
    /// IME underline distance from line bottom.
    std::optional<float> composition_underline_inset;
    /// Label/body font weight.
    std::optional<FontWeight> text_weight;
    /// Label/body font slant.
    std::optional<FontSlant> text_slant;
    /// Preferred font family.
    std::optional<std::string> font_family;
    /// Ordered fallback font families.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete multiline TextArea recipe.
///
/// Resolution order is base -> one interaction branch -> read_only -> focused.
/// Interaction precedence is disabled > pressed > hovered > normal. Inherited
/// values are applied before component-local values inside every layer.
///
/// Recipes are detached owned values and can outlive the Theme used to create
/// defaults. Mutating this value alone has no retained/UI side effects and does
/// not trigger callbacks or invalidation.
struct TextAreaStyle {
    /// Base patch applied before all state-specific patches.
    TextAreaStylePatch base;
    /// Hover interaction patch.
    TextAreaStylePatch hovered;
    /// Pressed interaction patch.
    TextAreaStylePatch pressed;
    /// Disabled interaction patch; wins over pressed/hovered.
    TextAreaStylePatch disabled;
    /// Read-only orthogonal patch applied after interaction.
    TextAreaStylePatch read_only;
    /// Focused orthogonal patch applied last.
    TextAreaStylePatch focused;
};

/// Concrete multiline editor presentation/measurement values after resolution.
///
/// The snapshot is fully owned and borrows nothing from recipes or Theme.
/// Default construction performs no implicit Theme lookup; fields omitted by both
/// recipes retain their zero/default values.
///
/// Numeric fields remain exactly as selected by recipe precedence, including
/// negative/non-finite payloads. Resolution does not silently clamp or replace
/// them; the consuming TextArea/renderer contract owns any later validation.
struct ResolvedTextAreaStyle {
    /// Resolved Editor field background color.
    Color field_fill{};
    /// Resolved Field border color.
    Color border{};
    /// Resolved Label color.
    Color label{};
    /// Resolved Body and preedit text color.
    Color text{};
    /// Resolved Placeholder color.
    Color placeholder{};
    /// Resolved Selection highlight color.
    Color selection{};
    /// Resolved Caret color.
    Color caret{};
    /// Resolved IME/preedit underline color.
    Color composition_underline{};
    /// Resolved Border stroke width in logical UI units.
    float border_width{};
    /// Resolved Field corner radius in logical UI units.
    float corner_radius{};
    /// Resolved Preferred measured control width in logical UI units.
    float control_width{};
    /// Resolved Preferred measured control height in logical UI units.
    float control_height{};
    /// Resolved Field top offset from control top in logical UI units.
    float field_top{};
    /// Resolved Minimum field height in logical UI units.
    float minimum_field_height{};
    /// Resolved Left/right content inset in logical UI units.
    float horizontal_padding{};
    /// Resolved Top/bottom content inset in logical UI units.
    float vertical_padding{};
    /// Resolved Label center-Y offset from control top.
    float label_offset_y{};
    /// Resolved Label font size in logical UI units.
    float label_size{};
    /// Resolved Body font size in logical UI units.
    float text_size{};
    /// Resolved Vertical advance per logical text line.
    float line_height{};
    /// Resolved Selection highlight radius.
    float selection_corner_radius{};
    /// Resolved Selection vertical inset within each line.
    float selection_vertical_inset{};
    /// Resolved Extra highlight width when selection includes a newline.
    float newline_selection_width{};
    /// Resolved Caret stroke width.
    float caret_width{};
    /// Resolved Caret top/bottom inset within a line.
    float caret_vertical_inset{};
    /// Resolved IME underline stroke width.
    float composition_underline_width{};
    /// Resolved IME underline distance from line bottom.
    float composition_underline_inset{};
    /// Resolved Label/body font weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Resolved Label/body font slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Resolved Preferred font family.
    std::string font_family;
    /// Resolved Ordered fallback font families.
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_text_area_style_patch(ResolvedTextAreaStyle& target,
                                        const TextAreaStylePatch& patch) {
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
    if (patch.minimum_field_height) target.minimum_field_height = *patch.minimum_field_height;
    if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
    if (patch.vertical_padding) target.vertical_padding = *patch.vertical_padding;
    if (patch.label_offset_y) target.label_offset_y = *patch.label_offset_y;
    if (patch.label_size) target.label_size = *patch.label_size;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.line_height) target.line_height = *patch.line_height;
    if (patch.selection_corner_radius) target.selection_corner_radius = *patch.selection_corner_radius;
    if (patch.selection_vertical_inset) target.selection_vertical_inset = *patch.selection_vertical_inset;
    if (patch.newline_selection_width) target.newline_selection_width = *patch.newline_selection_width;
    if (patch.caret_width) target.caret_width = *patch.caret_width;
    if (patch.caret_vertical_inset) target.caret_vertical_inset = *patch.caret_vertical_inset;
    if (patch.composition_underline_width) target.composition_underline_width = *patch.composition_underline_width;
    if (patch.composition_underline_inset) target.composition_underline_inset = *patch.composition_underline_inset;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_text_area_interaction_patch(ResolvedTextAreaStyle& target,
                                              const TextAreaStyle& style,
                                              InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_text_area_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_text_area_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_text_area_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Build the complete default TextArea recipe from a synchronously borrowed Theme.
///
/// The Theme is not retained. The returned recipe owns copied font-family/fallback
/// data and can outlive the Theme. Normal/hover/pressed/disabled/read-only/focus
/// defaults keep the baseline geometry stable and vary presentation only.
///
/// No callbacks, retained-tree mutation, or platform/native lookup occur.
/// Copying Theme-owned strings/vectors may allocate and propagate allocation
/// failure. This helper belongs to UI/style setup and is not audio-RT safe.
[[nodiscard]] inline TextAreaStyle default_text_area_style(const Theme& theme) {
    TextAreaStyle style;
    style.base.field_fill = theme.palette.control_background;
    style.base.border = theme.palette.border;
    style.base.label = theme.palette.muted_text;
    style.base.text = theme.palette.text;
    style.base.placeholder = theme.palette.muted_text;
    style.base.selection = theme.palette.selection;
    style.base.caret = colors::caret;
    style.base.composition_underline = theme.palette.focus;
    style.base.border_width = theme.controls.border_width;
    style.base.corner_radius = 9.0f;
    style.base.control_width = 420.0f;
    style.base.control_height = 180.0f;
    style.base.field_top = 24.0f;
    style.base.minimum_field_height = 46.0f;
    style.base.horizontal_padding = 12.0f;
    style.base.vertical_padding = 8.0f;
    style.base.label_offset_y = 10.0f;
    style.base.label_size = 12.0f;
    style.base.text_size = 15.0f;
    style.base.line_height = 22.0f;
    style.base.selection_corner_radius = 2.0f;
    style.base.selection_vertical_inset = 2.0f;
    style.base.newline_selection_width = 6.0f;
    style.base.caret_width = 1.5f;
    style.base.caret_vertical_inset = 2.0f;
    style.base.composition_underline_width = 1.5f;
    style.base.composition_underline_inset = 3.0f;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.hovered.border = theme.palette.control_hover;
    style.pressed.border = theme.palette.accent;
    style.disabled.label = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.disabled.placeholder = theme.palette.disabled;
    style.disabled.caret = theme.palette.disabled;
    style.read_only.text = theme.palette.muted_text;
    style.read_only.caret = theme.palette.muted_text;
    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolve inherited and component-local TextArea recipes for one VisualState.
///
/// Inputs are borrowed only during this call. Resolution is
/// base -> interaction -> read_only -> focused, with
/// disabled > pressed > hovered > normal interaction precedence and local fields
/// winning over inherited fields inside every layer. The returned snapshot owns
/// all copied font strings/vectors.
///
/// The resolver performs no Theme lookup, numeric sanitization, retained mutation,
/// invalidation, callback invocation, or reentrant UI dispatch. Fields omitted by
/// both recipes remain default constructed. String/vector copies may allocate and
/// allocation failure propagates.
[[nodiscard]] inline ResolvedTextAreaStyle resolve_text_area_style(
    const TextAreaStyle& inherited,
    const TextAreaStyle& explicit_style,
    const VisualState& state) {
    ResolvedTextAreaStyle resolved;
    detail::apply_text_area_style_patch(resolved, inherited.base);
    detail::apply_text_area_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_text_area_interaction_patch(resolved, inherited, interaction);
    detail::apply_text_area_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_text_area_style_patch(resolved, inherited.read_only);
        detail::apply_text_area_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_text_area_style_patch(resolved, inherited.focused);
        detail::apply_text_area_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
