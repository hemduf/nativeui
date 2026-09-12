#pragma once

#include <nativeui/style.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ui {

/// Typed ComboBox/PopupMenu anchor presentation and measurement overrides.
/// Interaction uses the common T038 branch; read-only and focus are orthogonal.
struct ComboBoxStylePatch {
    std::optional<Color> fill;
    std::optional<Color> border;
    std::optional<Color> text;
    std::optional<float> border_width;
    std::optional<float> corner_radius;
    std::optional<float> minimum_width;
    std::optional<float> control_height;
    std::optional<float> horizontal_padding;
    std::optional<float> text_size;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct ComboBoxStyle {
    ComboBoxStylePatch base;
    ComboBoxStylePatch hovered;
    ComboBoxStylePatch pressed;
    ComboBoxStylePatch disabled;
    ComboBoxStylePatch read_only;
    ComboBoxStylePatch focused;
};

struct ResolvedComboBoxStyle {
    Color fill{};
    Color border{};
    Color text{};
    float border_width{};
    float corner_radius{};
    float minimum_width{};
    float control_height{};
    float horizontal_padding{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;

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

/// Typed popup row presentation. Selected is orthogonal to the common
/// interaction branch so keyboard/pointer highlight can coexist predictably
/// with hover/pressed state while Disabled still wins interaction precedence.
struct MenuItemStylePatch {
    std::optional<Color> fill;
    std::optional<Color> text;
    std::optional<Color> separator;
    std::optional<float> row_height;
    std::optional<float> separator_height;
    std::optional<float> horizontal_padding;
    std::optional<float> separator_inset;
    std::optional<float> separator_width;
    std::optional<float> corner_radius;
    std::optional<float> text_size;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct MenuItemStyle {
    MenuItemStylePatch base;
    MenuItemStylePatch selected;
    MenuItemStylePatch hovered;
    MenuItemStylePatch pressed;
    MenuItemStylePatch disabled;
    MenuItemStylePatch read_only;
    MenuItemStylePatch focused;
};

struct ResolvedMenuItemStyle {
    Color fill{};
    Color text{};
    Color separator{};
    float row_height{};
    float separator_height{};
    float horizontal_padding{};
    float separator_inset{};
    float separator_width{};
    float corner_radius{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;

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

[[nodiscard]] inline ResolvedMenuItemStyle resolve_menu_item_style(
    const MenuItemStyle& inherited,
    const MenuItemStyle& explicit_style,
    const VisualState& state) {
    ResolvedMenuItemStyle resolved;
    detail::apply_menu_item_style_patch(resolved, inherited.base);
    detail::apply_menu_item_style_patch(resolved, explicit_style.base);

    if (state.selected) {
        detail::apply_menu_item_style_patch(resolved, inherited.selected);
        detail::apply_menu_item_style_patch(resolved, explicit_style.selected);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_menu_item_interaction_patch(resolved, inherited, interaction);
    detail::apply_menu_item_interaction_patch(resolved, explicit_style, interaction);

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
