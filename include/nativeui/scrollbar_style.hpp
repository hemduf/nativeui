#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Typed ScrollView scrollbar presentation/geometry overrides. The scrollbar
/// is pointer-targetable but deliberately not keyboard-focusable, so T038 uses
/// the common interaction branch plus the orthogonal read-only state here.
struct ScrollbarStylePatch {
    std::optional<Color> track;
    std::optional<Color> thumb;
    std::optional<float> thickness;
    std::optional<float> minimum_thumb;
    std::optional<float> corner_radius;
};

struct ScrollbarStyle {
    ScrollbarStylePatch base;
    ScrollbarStylePatch hovered;
    ScrollbarStylePatch pressed;
    ScrollbarStylePatch disabled;
    ScrollbarStylePatch read_only;
};

struct ResolvedScrollbarStyle {
    Color track{};
    Color thumb{};
    float thickness{};
    float minimum_thumb{};
    float corner_radius{};

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

/// T037-backed default ScrollView scrollbar recipe. The base preserves the
/// existing T034 8 px track, 18 px minimum thumb and 4 px radius contract.
/// Interaction variants are paint-only so normal/hover/pressed/disabled keep
/// identical geometry unless an application explicitly opts into a geometry
/// override in that state.
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

/// Resolve a scrollbar recipe from Theme/T039 inheritance, one local explicit
/// recipe and the common T038 state precedence. Read-only is orthogonal to the
/// competing disabled > pressed > hovered > normal interaction branch.
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
