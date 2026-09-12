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

/// Typed Checkbox style data. Checked state is orthogonal to the interaction
/// branch; disabled/pressed/hovered are then resolved with the same global
/// interaction precedence used by every T038 family.
struct CheckboxStylePatch {
    std::optional<Color> box_fill;
    std::optional<Color> box_border;
    std::optional<Color> checkmark;
    std::optional<Color> text;
    std::optional<float> box_size;
    std::optional<float> box_corner_radius;
    std::optional<float> box_border_width;
    std::optional<float> checkmark_width;
    std::optional<float> minimum_width;
    std::optional<float> control_height;
    std::optional<float> leading_padding;
    std::optional<float> label_gap;
    std::optional<float> text_size;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct CheckboxStyle {
    CheckboxStylePatch base;
    CheckboxStylePatch checked;
    CheckboxStylePatch hovered;
    CheckboxStylePatch pressed;
    CheckboxStylePatch disabled;
    CheckboxStylePatch read_only;
    CheckboxStylePatch focused;
};

struct ResolvedCheckboxStyle {
    Color box_fill{};
    Color box_border{};
    Color checkmark{};
    Color text{};
    float box_size{};
    float box_corner_radius{};
    float box_border_width{};
    float checkmark_width{};
    float minimum_width{};
    float control_height{};
    float leading_padding{};
    float label_gap{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_checkbox_style_patch(ResolvedCheckboxStyle& target,
                                       const CheckboxStylePatch& patch) {
    if (patch.box_fill) target.box_fill = *patch.box_fill;
    if (patch.box_border) target.box_border = *patch.box_border;
    if (patch.checkmark) target.checkmark = *patch.checkmark;
    if (patch.text) target.text = *patch.text;
    if (patch.box_size) target.box_size = *patch.box_size;
    if (patch.box_corner_radius) target.box_corner_radius = *patch.box_corner_radius;
    if (patch.box_border_width) target.box_border_width = *patch.box_border_width;
    if (patch.checkmark_width) target.checkmark_width = *patch.checkmark_width;
    if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
    if (patch.label_gap) target.label_gap = *patch.label_gap;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_checkbox_interaction_patch(ResolvedCheckboxStyle& target,
                                             const CheckboxStyle& style,
                                             InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_checkbox_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_checkbox_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_checkbox_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

[[nodiscard]] inline CheckboxStyle default_checkbox_style(const Theme& theme) {
    CheckboxStyle style;
    style.base.box_fill = theme.palette.control_background;
    style.base.box_border = theme.palette.border;
    style.base.checkmark = theme.palette.background;
    style.base.text = theme.palette.text;
    style.base.box_size = 18.0f;
    style.base.box_corner_radius = theme.radii.sm;
    style.base.box_border_width = theme.controls.border_width;
    style.base.checkmark_width = 2.0f;
    style.base.minimum_width = 72.0f;
    style.base.control_height = 32.0f;
    style.base.leading_padding = 3.0f;
    style.base.label_gap = 10.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.checked.box_fill = theme.palette.accent;
    style.pressed.box_border = theme.palette.accent;
    style.disabled.box_fill = theme.palette.control_background;
    style.disabled.box_border = theme.palette.border;
    style.disabled.checkmark = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.focused.box_border = theme.palette.focus;
    style.focused.box_border_width = theme.controls.focus_ring_width;
    return style;
}

[[nodiscard]] inline ResolvedCheckboxStyle resolve_checkbox_style(
    const CheckboxStyle& inherited,
    const CheckboxStyle& explicit_style,
    const VisualState& state) {
    ResolvedCheckboxStyle resolved;
    detail::apply_checkbox_style_patch(resolved, inherited.base);
    detail::apply_checkbox_style_patch(resolved, explicit_style.base);

    if (state.checked) {
        detail::apply_checkbox_style_patch(resolved, inherited.checked);
        detail::apply_checkbox_style_patch(resolved, explicit_style.checked);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_checkbox_interaction_patch(resolved, inherited, interaction);
    detail::apply_checkbox_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_checkbox_style_patch(resolved, inherited.read_only);
        detail::apply_checkbox_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_checkbox_style_patch(resolved, inherited.focused);
        detail::apply_checkbox_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

/// Radio buttons have distinct circular geometry but use the same VisualState
/// and interaction-resolution contract. Selected state is an orthogonal patch;
/// disabled is applied afterwards so selected+disabled remains visibly disabled.
struct RadioStylePatch {
    std::optional<Color> outer_fill;
    std::optional<Color> inner_fill;
    std::optional<Color> mark_fill;
    std::optional<Color> text;
    std::optional<float> outer_radius;
    std::optional<float> inner_radius;
    std::optional<float> mark_radius;
    std::optional<float> minimum_width;
    std::optional<float> control_height;
    std::optional<float> leading_padding;
    std::optional<float> label_gap;
    std::optional<float> text_size;
    std::optional<FontWeight> text_weight;
    std::optional<FontSlant> text_slant;
    std::optional<std::string> font_family;
    std::optional<std::vector<std::string>> fallback_families;
};

struct RadioStyle {
    RadioStylePatch base;
    RadioStylePatch selected;
    RadioStylePatch hovered;
    RadioStylePatch pressed;
    RadioStylePatch disabled;
    RadioStylePatch read_only;
    RadioStylePatch focused;
};

struct ResolvedRadioStyle {
    Color outer_fill{};
    Color inner_fill{};
    Color mark_fill{};
    Color text{};
    float outer_radius{};
    float inner_radius{};
    float mark_radius{};
    float minimum_width{};
    float control_height{};
    float leading_padding{};
    float label_gap{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string font_family;
    std::vector<std::string> fallback_families;
};

namespace detail {

inline void apply_radio_style_patch(ResolvedRadioStyle& target,
                                    const RadioStylePatch& patch) {
    if (patch.outer_fill) target.outer_fill = *patch.outer_fill;
    if (patch.inner_fill) target.inner_fill = *patch.inner_fill;
    if (patch.mark_fill) target.mark_fill = *patch.mark_fill;
    if (patch.text) target.text = *patch.text;
    if (patch.outer_radius) target.outer_radius = *patch.outer_radius;
    if (patch.inner_radius) target.inner_radius = *patch.inner_radius;
    if (patch.mark_radius) target.mark_radius = *patch.mark_radius;
    if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
    if (patch.control_height) target.control_height = *patch.control_height;
    if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
    if (patch.label_gap) target.label_gap = *patch.label_gap;
    if (patch.text_size) target.text_size = *patch.text_size;
    if (patch.text_weight) target.text_weight = *patch.text_weight;
    if (patch.text_slant) target.text_slant = *patch.text_slant;
    if (patch.font_family) target.font_family = *patch.font_family;
    if (patch.fallback_families) target.fallback_families = *patch.fallback_families;
}

inline void apply_radio_interaction_patch(ResolvedRadioStyle& target,
                                          const RadioStyle& style,
                                          InteractionVisualState interaction) {
    switch (interaction) {
        case InteractionVisualState::Normal:
            return;
        case InteractionVisualState::Hovered:
            apply_radio_style_patch(target, style.hovered);
            return;
        case InteractionVisualState::Pressed:
            apply_radio_style_patch(target, style.pressed);
            return;
        case InteractionVisualState::Disabled:
            apply_radio_style_patch(target, style.disabled);
            return;
    }
}

} // namespace detail

[[nodiscard]] inline RadioStyle default_radio_style(const Theme& theme) {
    RadioStyle style;
    style.base.outer_fill = theme.palette.border;
    style.base.inner_fill = theme.palette.control_background;
    style.base.mark_fill = theme.palette.accent;
    style.base.text = theme.palette.text;
    style.base.outer_radius = 9.0f;
    style.base.inner_radius = 7.0f;
    style.base.mark_radius = 4.0f;
    style.base.minimum_width = 72.0f;
    style.base.control_height = 32.0f;
    style.base.leading_padding = 3.0f;
    style.base.label_gap = 9.0f;
    style.base.text_size = theme.typography.control_size;
    style.base.text_weight = theme.typography.control_weight;
    style.base.text_slant = theme.typography.slant;
    style.base.font_family = theme.typography.family;
    style.base.fallback_families = theme.typography.fallback_families;

    style.pressed.outer_fill = theme.palette.accent;
    style.disabled.outer_fill = theme.palette.border;
    style.disabled.mark_fill = theme.palette.disabled;
    style.disabled.text = theme.palette.disabled;
    style.focused.outer_fill = theme.palette.focus;
    return style;
}

[[nodiscard]] inline ResolvedRadioStyle resolve_radio_style(
    const RadioStyle& inherited,
    const RadioStyle& explicit_style,
    const VisualState& state) {
    ResolvedRadioStyle resolved;
    detail::apply_radio_style_patch(resolved, inherited.base);
    detail::apply_radio_style_patch(resolved, explicit_style.base);

    if (state.selected) {
        detail::apply_radio_style_patch(resolved, inherited.selected);
        detail::apply_radio_style_patch(resolved, explicit_style.selected);
    }

    const auto interaction = resolve_interaction_state(state);
    detail::apply_radio_interaction_patch(resolved, inherited, interaction);
    detail::apply_radio_interaction_patch(resolved, explicit_style, interaction);

    if (state.read_only) {
        detail::apply_radio_style_patch(resolved, inherited.read_only);
        detail::apply_radio_style_patch(resolved, explicit_style.read_only);
    }
    if (state.focused) {
        detail::apply_radio_style_patch(resolved, inherited.focused);
        detail::apply_radio_style_patch(resolved, explicit_style.focused);
    }
    return resolved;
}

} // namespace ui
