#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Partial Slider/RangeSlider presentation and geometry override.
///
/// Disengaged optionals inherit the value already selected by preceding layers.
/// Scalar geometry uses logical UI pixels and is copied verbatim; this layer does
/// not clamp negative or non-finite values. The patch owns no UI/tree/native
/// resources and invokes no callbacks.
struct SliderStylePatch {
    /// Inactive track color.
    std::optional<Color> track;
    /// Active/range-filled track color.
    std::optional<Color> active;
    /// Thumb color.
    std::optional<Color> thumb;
    /// Focus-ring color.
    std::optional<Color> focus_ring;
    /// Formatter/readout text color.
    std::optional<Color> formatter_text;
    /// Track thickness in logical UI pixels.
    std::optional<float> track_thickness;
    /// Thumb diameter in logical UI pixels.
    std::optional<float> thumb_diameter;
    /// Focus-ring width in logical UI pixels.
    std::optional<float> focus_ring_width;
};

/// Complete Slider/RangeSlider style recipe.
///
/// Resolution order is base -> interaction -> read_only -> focused. Interaction
/// chooses one branch using disabled > pressed > hovered > normal precedence.
/// Inherited values are applied before component-local values inside each layer.
struct SliderStyle {
    /// Normal-state baseline.
    SliderStylePatch base;
    /// Overrides while hovered.
    SliderStylePatch hovered;
    /// Overrides while actively pressed.
    SliderStylePatch pressed;
    /// Overrides while disabled.
    SliderStylePatch disabled;
    /// Orthogonal read-only overrides.
    SliderStylePatch read_only;
    /// Orthogonal focus overrides applied last.
    SliderStylePatch focused;
};

/// Owned concrete Slider/RangeSlider style after recipe/state resolution.
///
/// Thumb diameter and focus-ring width currently contribute to intrinsic
/// cross-axis measurement; track thickness is paint geometry inside those bounds.
/// No field borrows from the recipes or Theme that produced this value.
struct ResolvedSliderStyle {
    /// Resolved inactive track color.
    Color track{};
    /// Resolved active/range-filled track color.
    Color active{};
    /// Resolved thumb color.
    Color thumb{};
    /// Resolved focus-ring color.
    Color focus_ring{};
    /// Resolved formatter/readout text color.
    Color formatter_text{};
    /// Resolved track thickness in logical UI pixels.
    float track_thickness{};
    /// Resolved thumb diameter in logical UI pixels.
    float thumb_diameter{};
    /// Resolved focus-ring width in logical UI pixels.
    float focus_ring_width{};
};

namespace detail {

enum class SliderStyleInvalidation {
    None,
    Paint,
    Layout,
};

[[nodiscard]] constexpr bool slider_style_equal(
    const ResolvedSliderStyle& lhs,
    const ResolvedSliderStyle& rhs) noexcept {
    return theme_color_equal(lhs.track, rhs.track) &&
           theme_color_equal(lhs.active, rhs.active) &&
           theme_color_equal(lhs.thumb, rhs.thumb) &&
           theme_color_equal(lhs.focus_ring, rhs.focus_ring) &&
           theme_color_equal(lhs.formatter_text, rhs.formatter_text) &&
           lhs.track_thickness == rhs.track_thickness &&
           lhs.thumb_diameter == rhs.thumb_diameter &&
           lhs.focus_ring_width == rhs.focus_ring_width;
}

[[nodiscard]] constexpr bool slider_layout_style_equal(
    const ResolvedSliderStyle& lhs,
    const ResolvedSliderStyle& rhs) noexcept {
    // Current Slider/RangeSlider measurement derives its cross-axis extent from
    // thumb diameter plus the focus-ring width. Track thickness remains paint
    // geometry inside the already measured bounds.
    return lhs.thumb_diameter == rhs.thumb_diameter &&
           lhs.focus_ring_width == rhs.focus_ring_width;
}

[[nodiscard]] constexpr SliderStyleInvalidation classify_slider_style_change(
    const ResolvedSliderStyle& previous,
    const ResolvedSliderStyle& next) noexcept {
    if (slider_style_equal(previous, next)) return SliderStyleInvalidation::None;
    if (!slider_layout_style_equal(previous, next)) return SliderStyleInvalidation::Layout;
    return SliderStyleInvalidation::Paint;
}

inline void apply_slider_style_patch(ResolvedSliderStyle& target,
                                     const SliderStylePatch& patch) {
    if (patch.track) target.track = *patch.track;
    if (patch.active) target.active = *patch.active;
    if (patch.thumb) target.thumb = *patch.thumb;
    if (patch.focus_ring) target.focus_ring = *patch.focus_ring;
    if (patch.formatter_text) target.formatter_text = *patch.formatter_text;
    if (patch.track_thickness) target.track_thickness = *patch.track_thickness;
    if (patch.thumb_diameter) target.thumb_diameter = *patch.thumb_diameter;
    if (patch.focus_ring_width) target.focus_ring_width = *patch.focus_ring_width;
}

inline void apply_slider_interaction_patch(ResolvedSliderStyle& target,
                                           const SliderStyle& style,
                                           InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_slider_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_slider_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_slider_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// Builds the default Slider/RangeSlider recipe from a borrowed Theme.
///
/// The Theme is read synchronously and is not retained. The returned recipe is
/// detached/owned. Default state variants change paint only; geometry remains
/// stable unless a caller explicitly overrides geometry in a state patch.
///
/// No callback or retained-tree mutation occurs. NativeUI does not promise this
/// style helper as an audio/DSP real-time-safe API.
[[nodiscard]] inline SliderStyle default_slider_style(const Theme& theme) {
    SliderStyle style;
    style.base.track = theme.palette.control_background;
    style.base.active = theme.palette.accent;
    style.base.thumb = theme.palette.accent;
    style.base.focus_ring = theme.palette.focus;
    style.base.formatter_text = theme.palette.muted_text;
    style.base.track_thickness = theme.controls.track_thickness;
    style.base.thumb_diameter = theme.controls.thumb_diameter;
    style.base.focus_ring_width = theme.controls.focus_ring_width;

    style.hovered.active = theme.palette.active_highlight;
    style.hovered.thumb = theme.palette.active_highlight;

    style.pressed.active = theme.palette.text;
    style.pressed.thumb = theme.palette.text;

    style.disabled.active = theme.palette.disabled;
    style.disabled.thumb = theme.palette.disabled;

    style.read_only.active = theme.palette.track;
    style.read_only.thumb = theme.palette.track;

    style.focused.focus_ring = theme.palette.focus;
    return style;
}

/// Resolves inherited and local Slider/RangeSlider recipes for one visual state.
///
/// Inputs are synchronous borrows; the returned snapshot is independent of them.
/// Resolution is base -> interaction -> read_only -> focused with local values
/// winning inherited values within every layer. No Theme lookup, callback,
/// retained mutation, or fallback path occurs. Fields omitted by both recipes
/// remain default-constructed in the result.
///
/// Applying the resulting style to a live widget remains UI/main-thread work;
/// this resolver carries no cross-thread synchronization or audio-RT guarantee.
[[nodiscard]] inline ResolvedSliderStyle resolve_slider_style(
    const SliderStyle& inherited,
    const SliderStyle& explicit_style,
    const VisualState& state) {
    ResolvedSliderStyle resolved;
    detail::apply_slider_style_patch(resolved, inherited.base);
    detail::apply_slider_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_slider_interaction_patch(resolved, inherited, interaction);
    detail::apply_slider_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_slider_style_patch(resolved, inherited.read_only);
        detail::apply_slider_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_slider_style_patch(resolved, inherited.focused);
        detail::apply_slider_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
