#pragma once

#include <nativeui/style.hpp>

#include <optional>

namespace ui {

/// Typed Slider/RangeSlider presentation overrides. Empty fields inherit from
/// the already resolved Theme/T039/component layer. Interaction variants share
/// the T038 VisualState precedence; read-only and focused are orthogonal.
struct SliderStylePatch {
    std::optional<Color> track;
    std::optional<Color> active;
    std::optional<Color> thumb;
    std::optional<Color> focus_ring;
    std::optional<Color> formatter_text;
    std::optional<float> track_thickness;
    std::optional<float> thumb_diameter;
    std::optional<float> focus_ring_width;
};

struct SliderStyle {
    SliderStylePatch base;
    SliderStylePatch hovered;
    SliderStylePatch pressed;
    SliderStylePatch disabled;
    SliderStylePatch read_only;
    SliderStylePatch focused;
};

struct ResolvedSliderStyle {
    Color track{};
    Color active{};
    Color thumb{};
    Color focus_ring{};
    Color formatter_text{};
    float track_thickness{};
    float thumb_diameter{};
    float focus_ring_width{};
};

namespace detail {

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

/// T037-backed default Slider/RangeSlider recipe. Interaction variants change
/// paint only; geometry stays stable across normal/hover/pressed/focus/read-only
/// and disabled states.
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

/// Resolve Slider/RangeSlider style from an already composed inherited recipe,
/// a local explicit recipe and the shared T038 visual-state model. T039 may
/// supply the inherited recipe without adding scope traversal here.
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
