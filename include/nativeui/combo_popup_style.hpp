#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ui {

/// Typed ComboBox/PopupMenu anchor presentation and measurement overrides.
/// Interaction uses the shared disabled > pressed > hovered > normal branch;
/// read-only and focus are orthogonal layers applied afterwards.
/// A disengaged optional inherits the value already produced by an earlier
/// layer. Engaged values replace it verbatim; geometry is not clamped or
/// checked for finiteness here. This is detached owned value data and holds no
/// retained-tree, native-view, or Theme borrow.
struct ComboBoxStylePatch {
    /// Anchor background color.
    std::optional<Color> fill;
    /// Anchor border color.
    std::optional<Color> border;
    /// Selected-value text color.
    std::optional<Color> text;
    /// Anchor border thickness in logical UI pixels.
    std::optional<float> border_width;
    /// Anchor corner radius in logical UI pixels.
    std::optional<float> corner_radius;
    /// Minimum measured anchor width in logical UI pixels.
    std::optional<float> minimum_width;
    /// Preferred anchor height in logical UI pixels.
    std::optional<float> control_height;
    /// Per-side horizontal inset between anchor bounds and text in logical UI pixels.
    std::optional<float> horizontal_padding;
    /// Selected-value text size in logical UI pixels.
    std::optional<float> text_size;
    /// Selected-value font weight.
    std::optional<FontWeight> text_weight;
    /// Selected-value font slant.
    std::optional<FontSlant> text_slant;
    /// Primary font family. The string is owned by the patch.
    std::optional<std::string> font_family;
    /// Ordered fallback font families. The vector and strings are owned by the patch.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete ComboBox anchor recipe with interaction/read-only/focus variants.
/// Resolution order is base -> interaction -> read_only -> focused. At each
/// layer inherited fields are applied before component-local fields, so local
/// values win on overlap. The recipe owns all strings/vectors it contains and
/// can outlive the Theme from which defaults were created.
struct ComboBoxStyle {
    /// Unconditional baseline patch.
    ComboBoxStylePatch base;
    /// Interaction patch selected when the anchor is hovered.
    ComboBoxStylePatch hovered;
    /// Interaction patch selected when the anchor is pressed.
    ComboBoxStylePatch pressed;
    /// Interaction patch selected when disabled; disabled wins interaction precedence.
    ComboBoxStylePatch disabled;
    /// Orthogonal read-only patch applied after interaction.
    ComboBoxStylePatch read_only;
    /// Final focus patch applied after read-only.
    ComboBoxStylePatch focused;
};

/// Concrete ComboBox visual and measurement values after style resolution.
/// This is an independent owned snapshot: it borrows neither input recipe nor
/// Theme. Default construction does not perform Theme lookup, so omitted fields
/// can remain zero/default when callers resolve incomplete recipes.
struct ResolvedComboBoxStyle {
    /// Resolved anchor background color.
    Color fill{};
    /// Resolved anchor border color.
    Color border{};
    /// Resolved selected-value text color.
    Color text{};
    /// Resolved border thickness in logical UI pixels.
    float border_width{};
    /// Resolved corner radius in logical UI pixels.
    float corner_radius{};
    /// Resolved minimum measured width in logical UI pixels.
    float minimum_width{};
    /// Resolved preferred height in logical UI pixels.
    float control_height{};
    /// Resolved per-side horizontal text inset in logical UI pixels.
    float horizontal_padding{};
    /// Resolved text size in logical UI pixels.
    float text_size{};
    /// Resolved font weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Resolved font slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned resolved primary font family.
    std::string font_family;
    /// Owned resolved fallback-family list.
    std::vector<std::string> fallback_families;

    /// Compares the resolved snapshot by value. Geometry/enums/strings use
    /// exact equality; colors use the toolkit Theme color-equivalence helper.
    [[nodiscard]] bool operator==(const ResolvedComboBoxStyle& other) const noexcept {
        return detail::theme_color_equal(fill, other.fill) &&
               detail::theme_color_equal(border, other.border) &&
               detail::theme_color_equal(text, other.text) &&
               border_width == other.border_width &&
               corner_radius == other.corner_radius &&
               minimum_width == other.minimum_width &&
               control_height == other.control_height &&
               horizontal_padding == other.horizontal_padding &&
               text_size == other.text_size &&
               text_weight == other.text_weight &&
               text_slant == other.text_slant &&
               font_family == other.font_family &&
               fallback_families == other.fallback_families;
    }
};

/// Typed popup-row presentation. A disengaged optional inherits the value
/// already resolved; an engaged value replaces it verbatim. Float geometry is
/// in logical UI pixels and is not clamped or validated by this value layer.
/// The patch owns its strings/vectors and carries no retained/native ownership.
struct MenuItemStylePatch {
    /// Popup-row background color.
    std::optional<Color> fill;
    /// Popup-row label color.
    std::optional<Color> text;
    /// Separator color for separator rows.
    std::optional<Color> separator;
    /// Preferred selectable-row height in logical UI pixels.
    std::optional<float> row_height;
    /// Preferred separator-row height in logical UI pixels.
    std::optional<float> separator_height;
    /// Per-side horizontal row-content inset in logical UI pixels.
    std::optional<float> horizontal_padding;
    /// Horizontal separator inset from row edges in logical UI pixels.
    std::optional<float> separator_inset;
    /// Separator stroke thickness in logical UI pixels.
    std::optional<float> separator_width;
    /// Row highlight/background corner radius in logical UI pixels.
    std::optional<float> corner_radius;
    /// Popup-row text size in logical UI pixels.
    std::optional<float> text_size;
    /// Popup-row font weight.
    std::optional<FontWeight> text_weight;
    /// Popup-row font slant.
    std::optional<FontSlant> text_slant;
    /// Primary font family. The string is owned by the patch.
    std::optional<std::string> font_family;
    /// Ordered fallback font families owned by the patch.
    std::optional<std::vector<std::string>> fallback_families;
};

/// Complete popup MenuItem recipe. Resolution order is
/// base -> interaction -> selected -> read_only -> focused. The interaction
/// branch itself follows disabled > pressed > hovered > normal. At every layer
/// inherited fields are applied before component-local fields, so local values
/// win when both recipes specify a field. Because selected is applied after the
/// interaction layer, selected values can override overlapping disabled/pressed
/// or hovered values.
struct MenuItemStyle {
    /// Unconditional baseline patch.
    MenuItemStylePatch base;
    /// Orthogonal selected-row patch applied after the interaction branch.
    MenuItemStylePatch selected;
    /// Interaction patch selected when hovered.
    MenuItemStylePatch hovered;
    /// Interaction patch selected when pressed.
    MenuItemStylePatch pressed;
    /// Interaction patch selected when disabled; disabled wins interaction precedence.
    MenuItemStylePatch disabled;
    /// Orthogonal read-only patch applied after selection.
    MenuItemStylePatch read_only;
    /// Final focus patch.
    MenuItemStylePatch focused;
};

/// Concrete popup-row visual and measurement values after style resolution.
/// The result owns its font strings/vectors and borrows nothing from recipes or
/// Theme. Default construction performs no Theme lookup.
struct ResolvedMenuItemStyle {
    /// Resolved popup-row background color.
    Color fill{};
    /// Resolved popup-row label color.
    Color text{};
    /// Resolved separator color.
    Color separator{};
    /// Resolved selectable-row height in logical UI pixels.
    float row_height{};
    /// Resolved separator-row height in logical UI pixels.
    float separator_height{};
    /// Resolved per-side horizontal content inset in logical UI pixels.
    float horizontal_padding{};
    /// Resolved separator inset in logical UI pixels.
    float separator_inset{};
    /// Resolved separator stroke thickness in logical UI pixels.
    float separator_width{};
    /// Resolved row corner radius in logical UI pixels.
    float corner_radius{};
    /// Resolved popup-row text size in logical UI pixels.
    float text_size{};
    /// Resolved font weight.
    FontWeight text_weight{FontWeight::Regular};
    /// Resolved font slant.
    FontSlant text_slant{FontSlant::Upright};
    /// Owned resolved primary font family.
    std::string font_family;
    /// Owned resolved fallback-family list.
    std::vector<std::string> fallback_families;

    /// Compares by value. Geometry/enums/strings use exact equality; colors use
    /// the toolkit Theme color-equivalence helper.
    [[nodiscard]] bool operator==(const ResolvedMenuItemStyle& other) const noexcept {
        return detail::theme_color_equal(fill, other.fill) &&
               detail::theme_color_equal(text, other.text) &&
               detail::theme_color_equal(separator, other.separator) &&
               row_height == other.row_height &&
               separator_height == other.separator_height &&
               horizontal_padding == other.horizontal_padding &&
               separator_inset == other.separator_inset &&
               separator_width == other.separator_width &&
               corner_radius == other.corner_radius &&
               text_size == other.text_size &&
               text_weight == other.text_weight &&
               text_slant == other.text_slant &&
               font_family == other.font_family &&
               fallback_families == other.fallback_families;
    }
};

namespace detail {

inline void apply_combo_box_style_patch(ResolvedComboBoxStyle& target,
                                        const ComboBoxStylePatch& patch) {
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

inline void apply_combo_box_interaction_patch(ResolvedComboBoxStyle& target,
                                              const ComboBoxStyle& style,
                                              InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_combo_box_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_combo_box_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_combo_box_style_patch(target, style.disabled);
            return;
    }
}

inline void apply_menu_item_style_patch(ResolvedMenuItemStyle& target,
                                        const MenuItemStylePatch& patch) {
    if (patch.fill) target.fill = *patch.fill;
    if (patch.text) target.text = *patch.text;
    if (patch.separator) target.separator = *patch.separator;
    if (patch.row_height) target.row_height = *patch.row_height;
    if (patch.separator_height) target.separator_height = *patch.separator_height;
    if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
    if (patch.separator_inset) target.separator_inset = *patch.separator_inset;
    if (patch.separator_width) target.separator_width = *patch.separator_width;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_menu_item_interaction_patch(ResolvedMenuItemStyle& target,
                                              const MenuItemStyle& style,
                                              InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_menu_item_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_menu_item_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_menu_item_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Builds the complete default ComboBox recipe from `theme`.
/// `theme` is borrowed only for this call; returned strings/vectors are owned
/// copies. The helper touches no UI/tree/native state and invokes no callbacks,
/// but copying dynamic data may allocate and propagate allocation exceptions.
/// It is therefore not suitable for an audio/DSP real-time callback.
[[nodiscard]] inline ComboBoxStyle default_combo_box_style(const Theme& theme) {
    ComboBoxStyle style;
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

    style.hovered.border = theme.palette.control_hover;
    style.pressed.border = theme.palette.accent;
    style.disabled.fill = theme.palette.control_background;
    style.disabled.border = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.read_only.border = theme.palette.track;
    style.read_only.text = theme.palette.muted_text;
    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolves inherited and component-local ComboBox recipes for `state`.
/// Inputs are synchronous borrows and the returned snapshot is fully owned.
/// Layering is base -> interaction -> read_only -> focused, with local fields
/// winning over inherited fields at each layer. No Theme lookup, numeric
/// sanitization, retained-state mutation, or callback/reentrant dispatch occurs.
/// String/vector copies may allocate; allocation failure propagates.
[[nodiscard]] inline ResolvedComboBoxStyle resolve_combo_box_style(
    const ComboBoxStyle& inherited,
    const ComboBoxStyle& explicit_style,
    const VisualState& state) {
    ResolvedComboBoxStyle resolved;
    detail::apply_combo_box_style_patch(resolved, inherited.base);
    detail::apply_combo_box_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_combo_box_interaction_patch(resolved, inherited, interaction);
    detail::apply_combo_box_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_combo_box_style_patch(resolved, inherited.read_only);
        detail::apply_combo_box_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_combo_box_style_patch(resolved, inherited.focused);
        detail::apply_combo_box_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

/// Builds the complete default popup MenuItem recipe from `theme`.
/// The Theme is borrowed only during the call and the returned recipe owns all
/// copied font-family data. No callbacks or retained/native mutation occur.
/// Dynamic copies may allocate and this helper is not audio-real-time safe.
[[nodiscard]] inline MenuItemStyle default_menu_item_style(const Theme& theme) {
    MenuItemStyle style;
    style.base.fill = Color{0.0f, 0.0f, 0.0f, 0.0f};
    style.base.text = theme.palette.text;
    style.base.separator = theme.palette.border;
    style.base.row_height = theme.controls.control_height;
    style.base.separator_height = theme.spacing.sm;
    style.base.horizontal_padding = theme.spacing.medium;
    style.base.separator_inset = theme.spacing.sm;
    style.base.separator_width = theme.controls.border_width;
    style.base.corner_radius = 0.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.selected.fill = theme.palette.selection;
    style.hovered.fill = theme.palette.selection;
    style.pressed.fill = theme.palette.active_highlight;
    style.disabled.text = theme.palette.disabled;
    style.read_only.text = theme.palette.muted_text;
    return style;
}

/// Resolves inherited and component-local popup MenuItem recipes for `state`.
/// Inputs are borrowed only for the call; the returned snapshot owns its data.
/// Layering is base -> interaction -> selected -> read_only -> focused, with
/// local values overriding inherited values within each layer. Numeric values
/// are not clamped. The resolver performs no Theme lookup, callback dispatch,
/// or retained-tree mutation, but owned string/vector copies may allocate.
[[nodiscard]] inline ResolvedMenuItemStyle resolve_menu_item_style(
    const MenuItemStyle& inherited,
    const MenuItemStyle& explicit_style,
    const VisualState& state) {
    ResolvedMenuItemStyle resolved;
    detail::apply_menu_item_style_patch(resolved, inherited.base);
    detail::apply_menu_item_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_menu_item_interaction_patch(resolved, inherited, interaction);
    detail::apply_menu_item_interaction_patch(resolved, explicit_style, interaction);

    if (state.selected) {
        detail::apply_menu_item_style_patch(resolved, inherited.selected);
        detail::apply_menu_item_style_patch(resolved, explicit_style.selected);
    }
    if (state.read_only) {
        detail::apply_menu_item_style_patch(resolved, inherited.read_only);
        detail::apply_menu_item_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_menu_item_style_patch(resolved, inherited.focused);
        detail::apply_menu_item_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
