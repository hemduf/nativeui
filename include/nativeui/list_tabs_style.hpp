#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Typed retained ListView surface/row presentation. Selection remains an
/// orthogonal VisualState flag; disabled/pressed/hovered use the common T038
/// interaction precedence. Geometry fields are explicit so invalidation can be
/// classified without string properties or backend state.
/// Each engaged optional overrides the current resolution layer; an absent
/// optional inherits the previous value. Float geometry uses logical UI
/// pixels and is stored verbatim without clamping or finiteness checks.
/// The patch is detached value data and owns no retained/native resources.
struct ListViewStylePatch {
    /// Background color of the ListView surface.
    std::optional<Color> surface_fill;
    /// Border color of the ListView surface.
    std::optional<Color> surface_border;
    /// Background color painted behind an individual row.
    std::optional<Color> row_fill;
    /// Accent color used by row selection/indicator decoration.
    std::optional<Color> row_accent;
    /// Header/tab separator color.
    std::optional<Color> separator;
    /// Surface border thickness in logical pixels.
    std::optional<float> surface_border_width;
    /// Surface corner radius in logical pixels.
    std::optional<float> surface_corner_radius;
    /// Horizontal row-content inset in logical pixels.
    std::optional<float> row_horizontal_inset;
    /// Vertical row-content inset in logical pixels.
    std::optional<float> row_vertical_inset;
    /// Row background corner radius in logical pixels.
    std::optional<float> row_corner_radius;
    /// Width of the row accent indicator in logical pixels.
    std::optional<float> row_accent_width;
    /// Horizontal accent inset in logical pixels.
    std::optional<float> row_accent_horizontal_inset;
    /// Vertical accent inset in logical pixels.
    std::optional<float> row_accent_vertical_inset;
    /// Separator stroke width in logical pixels.
    std::optional<float> separator_width;
    /// Separator inset in logical pixels.
    std::optional<float> separator_inset;
};

/// Complete ListView recipe. Selected/read-only/focused state remains
/// orthogonal to the shared interaction branch.
/// Resolution order is base -> selected -> interaction -> read_only -> focused.
/// At every layer inherited fields are applied before explicit/local fields,
/// so an explicit field wins when both recipes provide that same field.
struct ListViewStyle {
    /// Unconditional baseline patch.
    ListViewStylePatch base;
    /// Orthogonal patch applied when VisualState::selected is true.
    ListViewStylePatch selected;
    /// Interaction patch for hovered state.
    ListViewStylePatch hovered;
    /// Interaction patch for pressed state.
    ListViewStylePatch pressed;
    /// Interaction patch for disabled state; disabled wins interaction precedence.
    ListViewStylePatch disabled;
    /// Orthogonal patch applied after interaction when read-only.
    ListViewStylePatch read_only;
    /// Final orthogonal patch applied when focused.
    ListViewStylePatch focused;
};

/// Concrete ListView surface/row/separator presentation after resolution.
/// Fully owned snapshot: no member borrows from either recipe or Theme.
/// A default-constructed value is zero/default-valued and is not implicitly
/// populated from a Theme.
struct ResolvedListViewStyle {
    /// Resolved ListView surface fill.
    Color surface_fill{};
    /// Resolved ListView surface border color.
    Color surface_border{};
    /// Resolved row fill.
    Color row_fill{};
    /// Resolved row accent color.
    Color row_accent{};
    /// Resolved separator color.
    Color separator{};
    /// Resolved surface border thickness in logical pixels.
    float surface_border_width{};
    /// Resolved surface corner radius in logical pixels.
    float surface_corner_radius{};
    /// Resolved horizontal row inset in logical pixels.
    float row_horizontal_inset{};
    /// Resolved vertical row inset in logical pixels.
    float row_vertical_inset{};
    /// Resolved row corner radius in logical pixels.
    float row_corner_radius{};
    /// Resolved row-accent width in logical pixels.
    float row_accent_width{};
    /// Resolved horizontal row-accent inset in logical pixels.
    float row_accent_horizontal_inset{};
    /// Resolved vertical row-accent inset in logical pixels.
    float row_accent_vertical_inset{};
    /// Resolved separator width in logical pixels.
    float separator_width{};
    /// Resolved separator inset in logical pixels.
    float separator_inset{};

    /// Exact value comparison; no geometric tolerance is applied.
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
/// Engaged optionals replace the matching field for one resolution layer;
/// absent fields inherit. All float geometry and text sizes use logical UI
/// pixels and are stored verbatim without implicit validation or clamping.
struct TabsStylePatch {
    /// Header background color.
    std::optional<Color> header_fill;
    /// Header border color.
    std::optional<Color> header_border;
    /// Content-panel background color.
    std::optional<Color> panel_fill;
    /// Content-panel border color.
    std::optional<Color> panel_border;
    /// Individual tab background color.
    std::optional<Color> tab_fill;
    /// Tab label color.
    std::optional<Color> text;
    /// Header/tab separator color.
    std::optional<Color> separator;
    /// Selected-tab underline color.
    std::optional<Color> underline;
    /// Header border thickness in logical pixels.
    std::optional<float> header_border_width;
    /// Panel border thickness in logical pixels.
    std::optional<float> panel_border_width;
    /// Preferred header height in logical pixels.
    std::optional<float> header_height;
    /// Gap between header and panel in logical pixels.
    std::optional<float> panel_gap;
    /// Header corner radius in logical pixels.
    std::optional<float> header_corner_radius;
    /// Panel corner radius in logical pixels.
    std::optional<float> panel_corner_radius;
    /// Inset around each tab within the header in logical pixels.
    std::optional<float> tab_inset;
    /// Individual tab corner radius in logical pixels.
    std::optional<float> tab_corner_radius;
    /// Selected underline thickness in logical pixels.
    std::optional<float> underline_height;
    /// Selected underline inset from tab edges in logical pixels.
    std::optional<float> underline_inset;
    /// Separator stroke width in logical pixels.
    std::optional<float> separator_width;
    /// Separator inset in logical pixels.
    std::optional<float> separator_inset;
    /// Tab-label text size in logical pixels.
    std::optional<float> text_size;
};

/// Complete Tabs recipe for header, tabs, selected underline and panel.
/// Resolution order is base -> selected -> interaction -> read_only -> focused.
/// Recipes are owned backend-neutral values and resolver calls invoke no
/// callbacks or retained-tree mutation.
struct TabsStyle {
    /// Unconditional baseline patch.
    TabsStylePatch base;
    /// Patch applied when the tab is selected.
    TabsStylePatch selected;
    /// Interaction patch for hovered state.
    TabsStylePatch hovered;
    /// Interaction patch for pressed state.
    TabsStylePatch pressed;
    /// Interaction patch for disabled state.
    TabsStylePatch disabled;
    /// Orthogonal read-only patch applied after interaction.
    TabsStylePatch read_only;
    /// Final focus patch.
    TabsStylePatch focused;
};

/// Concrete Tabs presentation and measurement values after state resolution.
/// Fully owned presentation/measurement snapshot. No member borrows from
/// either recipe or the Theme used to create defaults.
struct ResolvedTabsStyle {
    /// Resolved header fill.
    Color header_fill{};
    /// Resolved header border color.
    Color header_border{};
    /// Resolved panel fill.
    Color panel_fill{};
    /// Resolved panel border color.
    Color panel_border{};
    /// Resolved tab fill.
    Color tab_fill{};
    /// Resolved tab-label color.
    Color text{};
    /// Resolved separator color.
    Color separator{};
    /// Resolved selected-underline color.
    Color underline{};
    /// Resolved header border thickness in logical pixels.
    float header_border_width{};
    /// Resolved panel border thickness in logical pixels.
    float panel_border_width{};
    /// Resolved preferred header height in logical pixels.
    float header_height{};
    /// Resolved header-to-panel gap in logical pixels.
    float panel_gap{};
    /// Resolved header corner radius in logical pixels.
    float header_corner_radius{};
    /// Resolved panel corner radius in logical pixels.
    float panel_corner_radius{};
    /// Resolved tab inset in logical pixels.
    float tab_inset{};
    /// Resolved tab corner radius in logical pixels.
    float tab_corner_radius{};
    /// Resolved selected-underline thickness in logical pixels.
    float underline_height{};
    /// Resolved underline inset in logical pixels.
    float underline_inset{};
    /// Resolved separator width in logical pixels.
    float separator_width{};
    /// Resolved separator inset in logical pixels.
    float separator_inset{};
    /// Resolved tab-label text size in logical pixels.
    float text_size{};

    /// Exact value comparison; no geometric tolerance is applied.
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

/// Builds a complete default ListView recipe from a Theme borrowed only for
/// this call. The returned recipe is independent owned value data.
/// This helper performs no callbacks or retained-tree mutation and is not
/// intended for audio/DSP real-time use.
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

/// Resolves inherited and explicit/local ListView recipes for a VisualState.
/// Inputs are borrowed only for this call; the result owns all values.
/// Layering is base -> selected -> interaction -> read_only -> focused.
/// No Theme lookup, callback, or numeric sanitization occurs here.
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

/// Builds a complete default Tabs recipe from a Theme borrowed only for this
/// call. The returned recipe owns its copied values and captures no UI/native
/// resource.
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

/// Resolves inherited and explicit/local Tabs recipes for a VisualState.
/// Inputs are synchronous borrows; the returned snapshot is independent.
/// Layering is base -> selected -> interaction -> read_only -> focused, with
/// explicit/local fields winning over inherited fields at each layer.
/// The resolver invokes no callbacks, performs no Theme lookup and does not
/// clamp numeric values.
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
