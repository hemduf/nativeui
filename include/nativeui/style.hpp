#pragma once

#include <nativeui/theme.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

/// Interaction branch shared by all T038 widget style resolvers. Focus,
/// selection/check state and read-only remain orthogonal flags on VisualState
/// and therefore do not erase the base interaction branch.
enum class InteractionVisualState {
    Normal,
    Hovered,
    Pressed,
    Disabled,
};

/// Backend-neutral logical/interaction snapshot consumed by typed widget style
/// resolvers. Widgets provide the flags they support; unsupported flags remain
/// false. No instance ownership or mutable global state is stored here.
struct VisualState {
    bool enabled{true};
    bool read_only{};
    bool hovered{};
    bool pressed{};
    bool focused{};
    bool selected{};
    bool checked{};

    [[nodiscard]] constexpr bool operator==(const VisualState&) const noexcept = default;
};

/// Fixed v1 interaction precedence: disabled > pressed > hovered > normal.
/// Orthogonal VisualState flags remain available to the caller for subsequent
/// typed style overlays.
[[nodiscard]] constexpr InteractionVisualState resolve_interaction_state(
    const VisualState& state) noexcept {
    if (!state.enabled) return InteractionVisualState::Disabled;
    if (state.pressed) return InteractionVisualState::Pressed;
    if (state.hovered) return InteractionVisualState::Hovered;
    return InteractionVisualState::Normal;
}

/// Optional Button presentation/measurement overrides. T038 deliberately uses
/// typed fields rather than string properties or runtime reflection. Empty
/// fields inherit the value already resolved by Theme/T039/component layers.
struct ButtonStylePatch {
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

/// Complete Button style recipe. `base` is applied first; one competing
/// interaction patch is then selected by disabled > pressed > hovered > normal.
/// Read-only and focus are orthogonal and are applied afterwards. T039 can pass
/// an inherited recipe as the first resolver argument without owning traversal
/// logic here.
struct ButtonStyle {
    ButtonStylePatch base;
    ButtonStylePatch hovered;
    ButtonStylePatch pressed;
    ButtonStylePatch disabled;
    ButtonStylePatch read_only;
    ButtonStylePatch focused;
};

/// Concrete Button values after Theme/inherited/component/state resolution.
struct ResolvedButtonStyle {
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
};

namespace detail {

inline void apply_button_style_patch(ResolvedButtonStyle& target,
                                     const ButtonStylePatch& patch) {
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

inline void apply_button_interaction_patch(ResolvedButtonStyle& target,
                                           const ButtonStyle& style,
                                           InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_button_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_button_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_button_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

/// T037-backed default Button recipe. The base matches the existing Button
/// geometry/presentation while default interaction variants preserve measured
/// geometry and change paint fields only.
[[nodiscard]] inline ButtonStyle default_button_style(const Theme& theme) {
    ButtonStyle style;
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

    style.hovered.fill = theme.palette.control_hover;

    style.pressed.fill = theme.palette.accent;
    style.pressed.border = theme.palette.accent;
    style.pressed.text = theme.palette.background;

    style.disabled.fill = theme.palette.control_background;
    style.disabled.text = theme.palette.disabled;

    style.focused.border = theme.palette.focus;
    style.focused.border_width = theme.controls.focus_ring_width;
    return style;
}

/// Resolve one Button style without any scope traversal. `inherited` is the
/// already-composed Theme/T039 recipe; `explicit_style` is local component
/// configuration. For each layer the inherited value is applied before the
/// component override, then the selected interaction and orthogonal overlays
/// are resolved deterministically.
[[nodiscard]] inline ResolvedButtonStyle resolve_button_style(
    const ButtonStyle& inherited,
    const ButtonStyle& explicit_style,
    const VisualState& state) {
    ResolvedButtonStyle resolved;
    detail::apply_button_style_patch(resolved, inherited.base);
    detail::apply_button_style_patch(resolved, explicit_style.base);

    const auto interaction = resolve_interaction_state(state);
    detail::apply_button_interaction_patch(resolved, inherited, interaction);
    detail::apply_button_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_button_style_patch(resolved, inherited.read_only);
        detail::apply_button_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_button_style_patch(resolved, inherited.focused);
        detail::apply_button_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
