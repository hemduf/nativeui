#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Partial presentation and intrinsic-size override shared by ProgressBar and Meter.
///
/// Every disengaged optional inherits the value produced by the preceding style
/// layer. Engaged numeric values are stored verbatim: this value layer does not
/// clamp negative or non-finite geometry. All scalar geometry and Size values use
/// logical UI pixels rather than framebuffer pixels.
///
/// The patch is detached value data. It owns no UI/tree/native resources, invokes
/// no callbacks, and can outlive the Theme or recipes from which it was copied.
struct ProgressStylePatch {
    /// Unfilled track/background color.
    std::optional<Color> track;
    /// Filled-progress color.
    std::optional<Color> fill;
    /// Outer border color.
    std::optional<Color> border;
    /// Formatted-value text color.
    std::optional<Color> text;
    /// Outer border width in logical UI pixels.
    std::optional<float> border_width;
    /// Outer control corner radius in logical UI pixels.
    std::optional<float> corner_radius;
    /// Filled-region corner radius in logical UI pixels.
    std::optional<float> fill_corner_radius;
    /// Formatted-value text size in logical UI pixels.
    std::optional<float> text_size;
    /// Preferred horizontal size without formatted text, in logical UI pixels.
    std::optional<Size> horizontal_size;
    /// Preferred horizontal size with formatted text, in logical UI pixels.
    std::optional<Size> horizontal_formatted_size;
    /// Preferred vertical size without formatted text, in logical UI pixels.
    std::optional<Size> vertical_size;
    /// Preferred vertical size with formatted text, in logical UI pixels.
    std::optional<Size> vertical_formatted_size;
};

/// Complete ProgressBar style recipe.
///
/// Resolution applies base first, then exactly one interaction branch
/// (disabled > pressed > hovered > normal), then read_only, then focused.
/// Within each layer inherited values are applied before component-local values.
struct ProgressBarStyle {
    /// Normal-state baseline.
    ProgressStylePatch base;
    /// Overrides while hovered.
    ProgressStylePatch hovered;
    /// Overrides while actively pressed.
    ProgressStylePatch pressed;
    /// Overrides while disabled.
    ProgressStylePatch disabled;
    /// Orthogonal read-only overrides applied after interaction.
    ProgressStylePatch read_only;
    /// Orthogonal focus overrides applied last.
    ProgressStylePatch focused;
};

/// Complete Meter style recipe.
///
/// Meter uses the same resolution precedence as ProgressBar but has independent
/// defaults, including a tighter default fill radius.
struct MeterStyle {
    /// Normal-state baseline.
    ProgressStylePatch base;
    /// Overrides while hovered.
    ProgressStylePatch hovered;
    /// Overrides while actively pressed.
    ProgressStylePatch pressed;
    /// Overrides while disabled.
    ProgressStylePatch disabled;
    /// Orthogonal read-only overrides applied after interaction.
    ProgressStylePatch read_only;
    /// Orthogonal focus overrides applied last.
    ProgressStylePatch focused;
};

/// Owned concrete ProgressBar/Meter style consumed by measurement and paint.
///
/// No field borrows from the input recipes or Theme used to produce it. Numeric
/// geometry remains exactly what resolution selected; no validation or fallback
/// is inserted at this layer.
struct ResolvedProgressStyle {
    /// Resolved unfilled track/background color.
    Color track{};
    /// Resolved filled-progress color.
    Color fill{};
    /// Resolved outer border color.
    Color border{};
    /// Resolved formatted-value text color.
    Color text{};
    /// Resolved outer border width in logical UI pixels.
    float border_width{};
    /// Resolved outer corner radius in logical UI pixels.
    float corner_radius{};
    /// Resolved filled-region radius in logical UI pixels.
    float fill_corner_radius{};
    /// Resolved formatted-value text size in logical UI pixels.
    float text_size{};
    /// Resolved preferred horizontal size without formatted text.
    Size horizontal_size{};
    /// Resolved preferred horizontal size with formatted text.
    Size horizontal_formatted_size{};
    /// Resolved preferred vertical size without formatted text.
    Size vertical_size{};
    /// Resolved preferred vertical size with formatted text.
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

/// Builds the default ProgressBar recipe from a synchronously borrowed Theme.
///
/// The returned recipe owns all copied values and does not retain the Theme.
/// Default geometry is expressed in logical UI pixels; interaction defaults do
/// not change geometry. No retained state is touched and no callbacks run.
///
/// This helper is intended for UI/style setup. NativeUI does not promise it as
/// an audio/DSP real-time-safe API.
[[nodiscard]] inline ProgressBarStyle default_progress_bar_style(const Theme& theme) {
    ProgressBarStyle style;
    detail::populate_progress_defaults(style.base, theme);
    style.base.fill_corner_radius = 6.0f;
    style.disabled.fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    return style;
}

/// Builds the default Meter recipe from a synchronously borrowed Theme.
///
/// The returned recipe is detached/owned. Meter shares ProgressBar geometry but
/// uses a 3 logical-pixel default fill radius. No UI callbacks or retained-tree
/// mutation occur; this helper is not an audio/DSP real-time contract.
[[nodiscard]] inline MeterStyle default_meter_style(const Theme& theme) {
    MeterStyle style;
    detail::populate_progress_defaults(style.base, theme);
    style.base.fill_corner_radius = 3.0f;
    style.disabled.fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    return style;
}

/// Resolves inherited and local ProgressBar recipes for one visual state.
///
/// Inputs are borrowed only for the duration of the call. Resolution order is
/// base -> interaction -> read_only -> focused, with inherited-before-local
/// precedence inside every layer. The returned value is fully owned.
///
/// The resolver performs no Theme lookup, retained mutation, callback dispatch,
/// or error fallback. If both recipes omit a field, that field keeps the
/// default-constructed value in ResolvedProgressStyle.
[[nodiscard]] inline ResolvedProgressStyle resolve_progress_bar_style(
    const ProgressBarStyle& inherited,
    const ProgressBarStyle& explicit_style,
    const VisualState& state) {
    return detail::resolve_progress_style(inherited, explicit_style, state);
}

/// Resolves inherited and local Meter recipes for one visual state.
///
/// Inputs are synchronous borrows and the result owns its values. Resolution is
/// deterministic: base -> interaction -> read_only -> focused, with local fields
/// winning inherited fields at the same layer. No callbacks, Theme lookup, or
/// retained-tree mutation occur.
[[nodiscard]] inline ResolvedProgressStyle resolve_meter_style(
    const MeterStyle& inherited,
    const MeterStyle& explicit_style,
    const VisualState& state) {
    return detail::resolve_progress_style(inherited, explicit_style, state);
}

} // namespace ui
