#pragma once

#include <nativeui/theme.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

/// Typed inheritable palette overrides for one lexical style scope.
struct StyleScopePaletteOverrides {
    std::optional<Color> background;
    std::optional<Color> surface;
    std::optional<Color> text;
    std::optional<Color> muted_text;
    std::optional<Color> border;
    std::optional<Color> accent;
    std::optional<Color> disabled;
    std::optional<Color> selection;
    std::optional<Color> focus;
    std::optional<Color> control_background;
    std::optional<Color> control_hover;
    std::optional<Color> active_highlight;
    std::optional<Color> track;
};

/// Typed inheritable typography overrides. These are style defaults only;
/// component/layout constraints are deliberately absent from the scope API.
struct StyleScopeTypographyOverrides {
    std::optional<std::string> family;
    std::optional<std::vector<std::string>> fallback_families;
    std::optional<float> base_size;
    std::optional<float> control_size;
    std::optional<float> label_size;
    std::optional<FontWeight> base_weight;
    std::optional<FontWeight> control_weight;
    std::optional<FontWeight> label_weight;
    std::optional<FontSlant> slant;

    [[nodiscard]] bool operator==(const StyleScopeTypographyOverrides&) const = default;
};

struct StyleScopeSpacingOverrides {
    std::optional<float> xs;
    std::optional<float> sm;
    std::optional<float> medium;
    std::optional<float> large;
    std::optional<float> xl;

    [[nodiscard]] bool operator==(const StyleScopeSpacingOverrides&) const = default;
};

struct StyleScopeRadiiOverrides {
    std::optional<float> sm;
    std::optional<float> medium;
    std::optional<float> large;

    [[nodiscard]] bool operator==(const StyleScopeRadiiOverrides&) const = default;
};

/// Inheritable standard-control defaults. These are the same typed defaults
/// already supplied by Theme; per-instance width/height/Flex/Grid/scroll state
/// and availability/callback/model state are intentionally not representable.
struct StyleScopeControlOverrides {
    std::optional<float> minimum_width;
    std::optional<float> control_height;
    std::optional<float> minimum_hit_target;
    std::optional<float> thumb_diameter;
    std::optional<float> track_thickness;
    std::optional<float> border_width;
    std::optional<float> focus_ring_width;

    [[nodiscard]] bool operator==(const StyleScopeControlOverrides&) const = default;
};

/// One lexical scope patch. Empty fields inherit from the nearest outer scope
/// or, ultimately, the owning UI Theme. Applying multiple values outer-to-inner
/// therefore implements nearest-scope-wins independently for each field.
struct StyleScopeOverrides {
    StyleScopePaletteOverrides palette;
    StyleScopeTypographyOverrides typography;
    StyleScopeSpacingOverrides spacing;
    StyleScopeRadiiOverrides radii;
    StyleScopeControlOverrides controls;
};

namespace detail {

template <class T>
inline void apply_scope_value(T& target, const std::optional<T>& value) {
    if (value) target = *value;
}

} // namespace detail

/// Apply one typed scope patch to an already-resolved inherited Theme value.
/// Call this outermost-to-innermost to obtain the lexical scope result. T038
/// component-local recipes still apply afterwards in each widget resolver.
[[nodiscard]] inline Theme apply_style_scope_overrides(
    Theme inherited, const StyleScopeOverrides& overrides) {
    detail::apply_scope_value(inherited.palette.background, overrides.palette.background);
    detail::apply_scope_value(inherited.palette.surface, overrides.palette.surface);
    detail::apply_scope_value(inherited.palette.text, overrides.palette.text);
    detail::apply_scope_value(inherited.palette.muted_text, overrides.palette.muted_text);
    detail::apply_scope_value(inherited.palette.border, overrides.palette.border);
    detail::apply_scope_value(inherited.palette.accent, overrides.palette.accent);
    detail::apply_scope_value(inherited.palette.disabled, overrides.palette.disabled);
    detail::apply_scope_value(inherited.palette.selection, overrides.palette.selection);
    detail::apply_scope_value(inherited.palette.focus, overrides.palette.focus);
    detail::apply_scope_value(
        inherited.palette.control_background, overrides.palette.control_background);
    detail::apply_scope_value(inherited.palette.control_hover, overrides.palette.control_hover);
    detail::apply_scope_value(
        inherited.palette.active_highlight, overrides.palette.active_highlight);
    detail::apply_scope_value(inherited.palette.track, overrides.palette.track);

    detail::apply_scope_value(inherited.typography.family, overrides.typography.family);
    detail::apply_scope_value(
        inherited.typography.fallback_families, overrides.typography.fallback_families);
    detail::apply_scope_value(inherited.typography.base_size, overrides.typography.base_size);
    detail::apply_scope_value(
        inherited.typography.control_size, overrides.typography.control_size);
    detail::apply_scope_value(inherited.typography.label_size, overrides.typography.label_size);
    detail::apply_scope_value(
        inherited.typography.base_weight, overrides.typography.base_weight);
    detail::apply_scope_value(
        inherited.typography.control_weight, overrides.typography.control_weight);
    detail::apply_scope_value(
        inherited.typography.label_weight, overrides.typography.label_weight);
    detail::apply_scope_value(inherited.typography.slant, overrides.typography.slant);

    detail::apply_scope_value(inherited.spacing.xs, overrides.spacing.xs);
    detail::apply_scope_value(inherited.spacing.sm, overrides.spacing.sm);
    detail::apply_scope_value(inherited.spacing.medium, overrides.spacing.medium);
    detail::apply_scope_value(inherited.spacing.large, overrides.spacing.large);
    detail::apply_scope_value(inherited.spacing.xl, overrides.spacing.xl);

    detail::apply_scope_value(inherited.radii.sm, overrides.radii.sm);
    detail::apply_scope_value(inherited.radii.medium, overrides.radii.medium);
    detail::apply_scope_value(inherited.radii.large, overrides.radii.large);

    detail::apply_scope_value(
        inherited.controls.minimum_width, overrides.controls.minimum_width);
    detail::apply_scope_value(
        inherited.controls.control_height, overrides.controls.control_height);
    detail::apply_scope_value(
        inherited.controls.minimum_hit_target, overrides.controls.minimum_hit_target);
    detail::apply_scope_value(
        inherited.controls.thumb_diameter, overrides.controls.thumb_diameter);
    detail::apply_scope_value(
        inherited.controls.track_thickness, overrides.controls.track_thickness);
    detail::apply_scope_value(
        inherited.controls.border_width, overrides.controls.border_width);
    detail::apply_scope_value(
        inherited.controls.focus_ring_width, overrides.controls.focus_ring_width);
    return inherited;
}

/// Classify the effective invalidation caused by replacing one scope patch
/// under a specific inherited Theme. Equal effective values are a strict no-op.
[[nodiscard]] inline ThemeInvalidation classify_style_scope_change(
    const Theme& inherited,
    const StyleScopeOverrides& before,
    const StyleScopeOverrides& after) {
    return classify_theme_change(
        apply_style_scope_overrides(inherited, before),
        apply_style_scope_overrides(inherited, after));
}

} // namespace ui
