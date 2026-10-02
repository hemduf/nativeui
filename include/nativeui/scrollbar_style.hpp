#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Partial ScrollView scrollbar presentation/geometry override.
///
/// Empty optionals inherit the value already selected by preceding style layers.
/// Geometry uses logical UI pixels and is stored verbatim; this layer performs no
/// clamping or finite-value validation. The value owns no UI/tree/native resource
/// and invokes no callbacks.
struct ScrollbarStylePatch {
    /// Scrollbar track color.
    std::optional<Color> track;
    /// Scrollbar thumb color.
    std::optional<Color> thumb;
    /// Cross-axis scrollbar thickness in logical UI pixels.
    std::optional<float> thickness;
    /// Minimum thumb length in logical UI pixels.
    std::optional<float> minimum_thumb;
    /// Track/thumb corner radius in logical UI pixels.
    std::optional<float> corner_radius;
};

/// Complete ScrollView scrollbar recipe.
///
/// The scrollbar is pointer-targetable but intentionally not keyboard-focusable,
/// so there is no focused patch. Resolution order is base -> interaction ->
/// read_only, with disabled > pressed > hovered > normal choosing the single
/// interaction branch and component-local values winning inherited values.
struct ScrollbarStyle {
    /// Normal-state baseline.
    ScrollbarStylePatch base;
    /// Overrides while hovered.
    ScrollbarStylePatch hovered;
    /// Overrides while actively pressed/dragged.
    ScrollbarStylePatch pressed;
    /// Overrides while disabled.
    ScrollbarStylePatch disabled;
    /// Orthogonal read-only overrides applied after interaction.
    ScrollbarStylePatch read_only;
};

/// Owned concrete scrollbar presentation and geometry after resolution.
///
/// No field borrows from input recipes or Theme. Numeric geometry remains exactly
/// the selected value; validation belongs to the consuming widget/layout path.
struct ResolvedScrollbarStyle {
    /// Resolved track color.
    Color track{};
    /// Resolved thumb color.
    Color thumb{};
    /// Resolved scrollbar thickness in logical UI pixels.
    float thickness{};
    /// Resolved minimum thumb length in logical UI pixels.
    float minimum_thumb{};
    /// Resolved track/thumb corner radius in logical UI pixels.
    float corner_radius{};

    /// Compares two resolved snapshots field-for-field.
    ///
    /// Colors use the style color comparator; floating-point geometry uses exact
    /// equality with no epsilon or normalization. The operation is constexpr,
    /// noexcept, allocation-free, and invokes no callbacks.
    [[nodiscard]] constexpr bool operator==(const ResolvedScrollbarStyle& other) const noexcept {
        return detail::theme_color_equal(track, other.track) &&
               detail::theme_color_equal(thumb, other.thumb) &&
               thickness == other.thickness &&
               minimum_thumb == other.minimum_thumb &&
               corner_radius == other.corner_radius;
    }
};

namespace detail {

inline void apply_scrollbar_style_patch(ResolvedScrollbarStyle& target,
                                        const ScrollbarStylePatch& patch) {
    if (patch.track) target.track = *patch.track;
    if (patch.thumb) target.thumb = *patch.thumb;
    if (patch.thickness) target.thickness = *patch.thickness;
    if (patch.minimum_thumb) target.minimum_thumb = *patch.minimum_thumb;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
}

inline void apply_scrollbar_interaction_patch(ResolvedScrollbarStyle& target,
                                              const ScrollbarStyle& style,
                                              InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_scrollbar_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_scrollbar_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_scrollbar_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Builds the default ScrollView scrollbar recipe from a borrowed Theme.
///
/// The Theme is read only for the call and is not retained. Defaults are an
/// 8-logical-pixel track, 18-logical-pixel minimum thumb, and Theme small radius.
/// Default interaction variants alter color only; geometry remains stable unless
/// callers explicitly override geometry in those patches.
///
/// No callbacks or retained-tree mutation occur. This helper is intended for
/// UI/style setup rather than audio/DSP real-time callbacks.
[[nodiscard]] inline ScrollbarStyle default_scrollbar_style(const Theme& theme) {
    ScrollbarStyle style;
    style.base.track = theme.palette.control_background;
    style.base.thumb = theme.palette.focus;
    style.base.thickness = 8.0f;
    style.base.minimum_thumb = 18.0f;
    style.base.corner_radius = theme.radii.sm;

    style.hovered.thumb = theme.palette.control_hover;
    style.pressed.thumb = theme.palette.accent;
    style.disabled.thumb = theme.palette.disabled;
    return style;
}

/// Resolves inherited and local scrollbar recipes for one visual state.
///
/// Inputs are borrowed synchronously; the result is fully owned. Resolution is
/// base -> interaction -> read_only. Local fields win inherited fields at each
/// layer. There is no focus layer because the scrollbar itself is not
/// keyboard-focusable.
///
/// The resolver performs no implicit Theme lookup, callbacks, retained mutation,
/// validation, or fallback. Fields omitted by both recipes remain
/// default-constructed in the returned snapshot.
[[nodiscard]] inline ResolvedScrollbarStyle resolve_scrollbar_style(
    const ScrollbarStyle& inherited,
    const ScrollbarStyle& explicit_style,
    const VisualState& state) {
    ResolvedScrollbarStyle resolved;
    detail::apply_scrollbar_style_patch(resolved, inherited.base);
    detail::apply_scrollbar_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_scrollbar_interaction_patch(resolved, inherited, interaction);
    detail::apply_scrollbar_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_scrollbar_style_patch(resolved, inherited.read_only);
        detail::apply_scrollbar_style_patch(resolved, explicit_style.read_only);
    }
    return resolved;
}

} // namespace ui
