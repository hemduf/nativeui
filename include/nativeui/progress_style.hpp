#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Shared typed presentation fields for display-only progress controls. Empty
/// fields inherit from the already-resolved Theme/T039/component layer.
struct ProgressStylePatch {
    std::optional<Color> track;
    std::optional<Color> fill;
    std::optional<Color> border;
    std::optional<Color> text;
    std::optional<float> border_width;
    std::optional<float> corner_radius;
    std::optional<float> fill_corner_radius;
    std::optional<float> text_size;
    std::optional<Size> horizontal_size;
    std::optional<Size> horizontal_formatted_size;
    std::optional<Size> vertical_size;
    std::optional<Size> vertical_formatted_size;
};

struct ProgressBarStyle {
    ProgressStylePatch base;
    ProgressStylePatch hovered;
    ProgressStylePatch pressed;
    ProgressStylePatch disabled;
    ProgressStylePatch read_only;
    ProgressStylePatch focused;
};

struct MeterStyle {
    ProgressStylePatch base;
    ProgressStylePatch hovered;
    ProgressStylePatch pressed;
    ProgressStylePatch disabled;
    ProgressStylePatch read_only;
    ProgressStylePatch focused;
};

struct ResolvedProgressStyle {
    Color track{};
    Color fill{};
    Color border{};
    Color text{};
    float border_width{};
    float corner_radius{};
    float fill_corner_radius{};
    float text_size{};
    Size horizontal_size{};
    Size horizontal_formatted_size{};
    Size vertical_size{};
    Size vertical_formatted_size{};
};

namespace detail {

inline void apply_progress_style_patch(ResolvedProgressStyle& target,
                                       const ProgressStylePatch& patch) {
    if (patch.track) target.track = *patch.track;
    if (patch.fill) target.fill = *patch.fill;
    if (patch.border) target.border = *patch.border;
    if (patch.text) target.text = *patch.text;
    if (patch.border_width) target.border_width = *patch.border_width;
    if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
    if (patch.fill_corner_radius) target.fill_corner_radius = *patch.fill_corner_radius;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.horizontal_size) target.horizontal_size = *patch.horizontal_size;
    if (patch.horizontal_formatted_size) {
        target.horizontal_formatted_size = *patch.horizontal_formatted_size;
    }
    if (patch.vertical_size) target.vertical_size = *patch.vertical_size;
    if (patch.vertical_formatted_size) {
        target.vertical_formatted_size = *patch.vertical_formatted_size;
    }
}

template <typename Style>
inline void apply_progress_interaction_patch(ResolvedProgressStyle& target,
                                             const Style& style,
                                             InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_progress_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_progress_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_progress_style_patch(target, style.disabled);
            return;
    }
}

template <typename Style>
[[nodiscard]] inline ResolvedProgressStyle resolve_progress_style(
    const Style& inherited,
    const Style& explicit_style,
    const VisualState& state) {
    ResolvedProgressStyle resolved;
    apply_progress_style_patch(resolved, inherited.base);
    apply_progress_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    apply_progress_interaction_patch(resolved, inherited, interaction);
    apply_progress_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        apply_progress_style_patch(resolved, inherited.read_only);
        apply_progress_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        apply_progress_style_patch(resolved, inherited.focused);
        apply_progress_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

inline void populate_progress_defaults(ProgressStylePatch& base, const Theme& theme) {
    base.track = theme.palette.control_background;
    base.fill = theme.palette.accent;
    base.border = theme.palette.border;
    base.text = theme.palette.text;
    base.border_width = theme.controls.border_width;
    base.corner_radius = 6.0f;
    base.text_size = 11.0f;
    base.horizontal_size = Size{176.0f, 24.0f};
    base.horizontal_formatted_size = Size{176.0f, 40.0f};
    base.vertical_size = Size{24.0f, 144.0f};
    base.vertical_formatted_size = Size{64.0f, 144.0f};
}

} // namespace detail

/// T037-backed ProgressBar defaults preserve the pre-T038 geometry and paint
/// contract. Interaction variants do not change geometry by default.
[[nodiscard]] inline ProgressBarStyle default_progress_bar_style(const Theme& theme) {
    ProgressBarStyle style;
    detail::populate_progress_defaults(style.base, theme);
    style.base.fill_corner_radius = 6.0f;
    style.disabled.fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    return style;
}

/// Meter shares the same field model, but retains its tighter fill radius.
[[nodiscard]] inline MeterStyle default_meter_style(const Theme& theme) {
    MeterStyle style;
    detail::populate_progress_defaults(style.base, theme);
    style.base.fill_corner_radius = 3.0f;
    style.disabled.fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    return style;
}

[[nodiscard]] inline ResolvedProgressStyle resolve_progress_bar_style(
    const ProgressBarStyle& inherited,
    const ProgressBarStyle& explicit_style,
    const VisualState& state) {
    return detail::resolve_progress_style(inherited, explicit_style, state);
}

[[nodiscard]] inline ResolvedProgressStyle resolve_meter_style(
    const MeterStyle& inherited,
    const MeterStyle& explicit_style,
    const VisualState& state) {
    return detail::resolve_progress_style(inherited, explicit_style, state);
}

} // namespace ui
