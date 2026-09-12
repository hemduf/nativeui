#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Typed retained ListView surface/row presentation. Selection remains an
/// orthogonal VisualState flag; disabled/pressed/hovered use the common T038
/// interaction precedence. Geometry fields are explicit so invalidation can be
/// classified without string properties or backend state.
struct ListViewStylePatch {
    std::optional<Color> surface_fill;
    std::optional<Color> surface_border;
    std::optional<Color> row_fill;
    std::optional<Color> row_accent;
    std::optional<Color> separator;
    std::optional<float> surface_border_width;
    std::optional<float> surface_corner_radius;
    std::optional<float> row_horizontal_inset;
    std::optional<float> row_vertical_inset;
    std::optional<float> row_corner_radius;
    std::optional<float> row_accent_width;
    std::optional<float> row_accent_horizontal_inset;
    std::optional<float> row_accent_vertical_inset;
    std::optional<float> separator_width;
    std::optional<float> separator_inset;
};

struct ListViewStyle {
    ListViewStylePatch base;
    ListViewStylePatch selected;
    ListViewStylePatch hovered;
    ListViewStylePatch pressed;
    ListViewStylePatch disabled;
    ListViewStylePatch read_only;
    ListViewStylePatch focused;
};

struct ResolvedListViewStyle {
    Color surface_fill{};
    Color surface_border{};
    Color row_fill{};
    Color row_accent{};
    Color separator{};
    float surface_border_width{};
    float surface_corner_radius{};
    float row_horizontal_inset{};
    float row_vertical_inset{};
    float row_corner_radius{};
    float row_accent_width{};
    float row_accent_horizontal_inset{};
    float row_accent_vertical_inset{};
    float separator_width{};
    float separator_inset{};

    [[nodiscard]] bool operator==(const ResolvedListViewStyle& other) const noexcept {
        return detail::theme_color_equal(surface_fill, other.surface_fill) &&
               detail::theme_color_equal(surface_border, other.surface_border) &&
               detail::theme_color_equal(row_fill, other.row_fill) &&
               detail::theme_color_equal(row_accent, other.row_accent) &&
               detail::theme_color_equal(separator, other.separator) &&
               surface_border_width == other.surface_border_width &&
               surface_corner_radius == other.surface_corner_radius &&
               row_horizontal_inset == other.row_horizontal_inset &&
               row_vertical_inset == other.row_vertical_inset &&
               row_corner_radius == other.row_corner_radius &&
               row_accent_width == other.row_accent_width &&
               row_accent_horizontal_inset == other.row_accent_horizontal_inset &&
               row_accent_vertical_inset == other.row_accent_vertical_inset &&
               separator_width == other.separator_width &&
               separator_inset == other.separator_inset;
    }
};

/// Typed Tabs header/panel presentation. Selected is orthogonal to the common
/// interaction branch; focus/read-only remain independent overlays. Default
/// interaction patches intentionally preserve all geometry fields.
struct TabsStylePatch {
    std::optional<Color> header_fill;
    std::optional<Color> header_border;
    std::optional<Color> panel_fill;
    std::optional<Color> panel_border;
    std::optional<Color> tab_fill;
    std::optional<Color> text;
    std::optional<Color> separator;
    std::optional<Color> underline;
    std::optional<float> header_border_width;
    std::optional<float> panel_border_width;
    std::optional<float> header_height;
    std::optional<float> panel_gap;
    std::optional<float> header_corner_radius;
    std::optional<float> panel_corner_radius;
    std::optional<float> tab_inset;
    std::optional<float> tab_corner_radius;
    std::optional<float> underline_height;
    std::optional<float> underline_inset;
    std::optional<float> separator_width;
    std::optional<float> separator_inset;
    std::optional<float> text_size;
};

struct TabsStyle {
    TabsStylePatch base;
    TabsStylePatch selected;
    TabsStylePatch hovered;
    TabsStylePatch pressed;
    TabsStylePatch disabled;
    TabsStylePatch read_only;
    TabsStylePatch focused;
};

struct ResolvedTabsStyle {
    Color header_fill{};
    Color header_border{};
    Color panel_fill{};
    Color panel_border{};
    Color tab_fill{};
    Color text{};
    Color separator{};
    Color underline{};
    float header_border_width{};
    float panel_border_width{};
    float header_height{};
    float panel_gap{};
    float header_corner_radius{};
    float panel_corner_radius{};
    float tab_inset{};
    float tab_corner_radius{};
    float underline_height{};
    float underline_inset{};
    float separator_width{};
    float separator_inset{};
    float text_size{};

    [[nodiscard]] bool operator==(const ResolvedTabsStyle& other) const noexcept {
        return detail::theme_color_equal(header_fill, other.header_fill) &&
               detail::theme_color_equal(header_border, other.header_border) &&
               detail::theme_color_equal(panel_fill, other.panel_fill) &&
               detail::theme_color_equal(panel_border, other.panel_border) &&
               detail::theme_color_equal(tab_fill, other.tab_fill) &&
               detail::theme_color_equal(text, other.text) &&
               detail::theme_color_equal(separator, other.separator) &&
               detail::theme_color_equal(underline, other.underline) &&
               header_border_width == other.header_border_width &&
               panel_border_width == other.panel_border_width &&
               header_height == other.header_height &&
               panel_gap == other.panel_gap &&
               header_corner_radius == other.header_corner_radius &&
               panel_corner_radius == other.panel_corner_radius &&
               tab_inset == other.tab_inset &&
               tab_corner_radius == other.tab_corner_radius &&
               underline_height == other.underline_height &&
               underline_inset == other.underline_inset &&
               separator_width == other.separator_width &&
               separator_inset == other.separator_inset &&
               text_size == other.text_size;
    }
};

namespace detail {

inline void apply_list_view_style_patch(ResolvedListViewStyle& target,
                                        const ListViewStylePatch& patch) {
    if (patch.surface_fill) target.surface_fill = *patch.surface_fill;
    if (patch.surface_border) target.surface_border = *patch.surface_border;
    if (patch.row_fill) target.row_fill = *patch.row_fill;
    if (patch.row_accent) target.row_accent = *patch.row_accent;
    if (patch.separator) target.separator = *patch.separator;
    if (patch.surface_border_width) target.surface_border_width = *patch.surface_border_width;
    if (patch.surface_corner_radius) target.surface_corner_radius = *patch.surface_corner_radius;
    if (patch.row_horizontal_inset) target.row_horizontal_inset = *patch.row_horizontal_inset;
    if (patch.row_vertical_inset) target.row_vertical_inset = *patch.row_vertical_inset;
    if (patch.row_corner_radius) target.row_corner_radius = *patch.row_corner_radius;
    if (patch.row_accent_width) target.row_accent_width = *patch.row_accent_width;
    if (patch.row_accent_horizontal_inset) {
        target.row_accent_horizontal_inset = *patch.row_accent_horizontal_inset;
    }
    if (patch.row_accent_vertical_inset) {
        target.row_accent_vertical_inset = *patch.row_accent_vertical_inset;
    }
    if (patch.separator_width) target.separator_width = *patch.separator_width;
    if (patch.separator_inset) target.separator_inset = *patch.separator_inset;
}

inline void apply_list_view_interaction_patch(ResolvedListViewStyle& target,
                                              const ListViewStyle& style,
                                              InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_list_view_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_list_view_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_list_view_style_patch(target, style.disabled);
            return;
    }
}

inline void apply_tabs_style_patch(ResolvedTabsStyle& target,
                                   const TabsStylePatch& patch) {
    if (patch.header_fill) target.header_fill = *patch.header_fill;
    if (patch.header_border) target.header_border = *patch.header_border;
    if (patch.panel_fill) target.panel_fill = *patch.panel_fill;
    if (patch.panel_border) target.panel_border = *patch.panel_border;
    if (patch.tab_fill) target.tab_fill = *patch.tab_fill;
    if (patch.text) target.text = *patch.text;
    if (patch.separator) target.separator = *patch.separator;
    if (patch.underline) target.underline = *patch.underline;
    if (patch.header_border_width) target.header_border_width = *patch.header_border_width;
    if (patch.panel_border_width) target.panel_border_width = *patch.panel_border_width;
    if (patch.header_height) target.header_height = *patch.header_height;
    if (patch.panel_gap) target.panel_gap = *patch.panel_gap;
    if (patch.header_corner_radius) target.header_corner_radius = *patch.header_corner_radius;
    if (patch.panel_corner_radius) target.panel_corner_radius = *patch.panel_corner_radius;
    if (patch.tab_inset) target.tab_inset = *patch.tab_inset;
    if (patch.tab_corner_radius) target.tab_corner_radius = *patch.tab_corner_radius;
    if (patch.underline_height) target.underline_height = *patch.underline_height;
    if (patch.underline_inset) target.underline_inset = *patch.underline_inset;
    if (patch.separator_width) target.separator_width = *patch.separator_width;
    if (patch.separator_inset) target.separator_inset = *patch.separator_inset;
    if (patch.text_size) target.text_size = *patch.text_size;
}

inline void apply_tabs_interaction_patch(ResolvedTabsStyle& target,
                                         const TabsStyle& style,
                                         InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_tabs_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_tabs_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_tabs_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

[[nodiscard]] inline ListViewStyle default_list_view_style(const Theme& theme) {
    ListViewStyle style;
    style.base.surface_fill = theme.palette.surface;
    style.base.surface_border = theme.palette.border;
    style.base.row_fill = Color{0.0f, 0.0f, 0.0f, 0.0f};
    style.base.row_accent = theme.palette.accent;
    style.base.separator = Color{0.20f, 0.22f, 0.25f, 0.70f};
    style.base.surface_border_width = theme.controls.border_width;
    style.base.surface_corner_radius = 9.0f;
    style.base.row_horizontal_inset = 4.0f;
    style.base.row_vertical_inset = 2.0f;
    style.base.row_corner_radius = 6.0f;
    style.base.row_accent_width = 3.0f;
    style.base.row_accent_horizontal_inset = 3.0f;
    style.base.row_accent_vertical_inset = 7.0f;
    style.base.separator_width = 1.0f;
    style.base.separator_inset = theme.spacing.medium;
    style.selected.row_fill = theme.palette.selection;
    style.hovered.row_fill = Color{0.145f, 0.155f, 0.175f, 1.0f};
    style.disabled.row_fill = Color{0.0f, 0.0f, 0.0f, 0.0f};
    style.focused.surface_border = theme.palette.focus;
    style.focused.surface_border_width = 1.5f;
    return style;
}

[[nodiscard]] inline ResolvedListViewStyle resolve_list_view_style(
    const ListViewStyle& inherited,
    const ListViewStyle& explicit_style,
    const VisualState& state) {
    ResolvedListViewStyle resolved;
    detail::apply_list_view_style_patch(resolved, inherited.base);
    detail::apply_list_view_style_patch(resolved, explicit_style.base);

    if (state.selected) {
        detail::apply_list_view_style_patch(resolved, inherited.selected);
        detail::apply_list_view_style_patch(resolved, explicit_style.selected);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_list_view_interaction_patch(resolved, inherited, interaction);
    detail::apply_list_view_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_list_view_style_patch(resolved, inherited.read_only);
        detail::apply_list_view_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_list_view_style_patch(resolved, inherited.focused);
        detail::apply_list_view_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

[[nodiscard]] inline TabsStyle default_tabs_style(const Theme& theme) {
    TabsStyle style;
    style.base.header_fill = theme.palette.surface;
    style.base.header_border = theme.palette.border;
    style.base.panel_fill = theme.palette.surface;
    style.base.panel_border = theme.palette.border;
    style.base.tab_fill = Color{0.0f, 0.0f, 0.0f, 0.0f};
    style.base.text = theme.palette.muted_text;
    style.base.separator = Color{0.20f, 0.22f, 0.25f, 0.75f};
    style.base.underline = theme.palette.accent;
    style.base.header_border_width = theme.controls.border_width;
    style.base.panel_border_width = theme.controls.border_width;
    style.base.header_height = 40.0f;
    style.base.panel_gap = 10.0f;
    style.base.header_corner_radius = theme.radii.medium;
    style.base.panel_corner_radius = 10.0f;
    style.base.tab_inset = 3.0f;
    style.base.tab_corner_radius = 6.0f;
    style.base.underline_height = 2.0f;
    style.base.underline_inset = 14.0f;
    style.base.separator_width = 1.0f;
    style.base.separator_inset = 10.0f;
    style.base.text_size = theme.typography.control_size;
    style.selected.tab_fill = theme.palette.control_background;
    style.selected.text = theme.palette.text;
    style.hovered.tab_fill = Color{0.145f, 0.155f, 0.175f, 1.0f};
    style.hovered.text = theme.palette.text;
    style.disabled.text = theme.palette.disabled;
    style.focused.header_border = theme.palette.focus;
    style.focused.header_border_width = 1.5f;
    return style;
}

[[nodiscard]] inline ResolvedTabsStyle resolve_tabs_style(
    const TabsStyle& inherited,
    const TabsStyle& explicit_style,
    const VisualState& state) {
    ResolvedTabsStyle resolved;
    detail::apply_tabs_style_patch(resolved, inherited.base);
    detail::apply_tabs_style_patch(resolved, explicit_style.base);

    if (state.selected) {
        detail::apply_tabs_style_patch(resolved, inherited.selected);
        detail::apply_tabs_style_patch(resolved, explicit_style.selected);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_tabs_interaction_patch(resolved, inherited, interaction);
    detail::apply_tabs_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_tabs_style_patch(resolved, inherited.read_only);
        detail::apply_tabs_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_tabs_style_patch(resolved, inherited.focused);
        detail::apply_tabs_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
