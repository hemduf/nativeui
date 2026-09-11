#pragma once

#include <nativeui/text.hpp>

#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

[[nodiscard]] constexpr bool theme_color_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

} // namespace detail

struct ThemePalette {
    Color background{colors::background};
    Color surface{colors::panel};
    Color text{colors::text};
    Color muted_text{colors::textMuted};
    Color border{colors::border};
    Color accent{colors::accent};
    Color disabled{colors::textMuted};
    Color selection{colors::selection};
    Color focus{colors::borderFocus};

    // Shared interaction colors used by the existing standard controls. T038
    // owns per-widget state patches; these remain global theme tokens only.
    Color control_background{colors::input};
    Color control_hover{colors::knob};
    Color active_highlight{colors::caret};
    Color track{colors::track};

    [[nodiscard]] constexpr bool operator==(const ThemePalette& other) const noexcept {
        return detail::theme_color_equal(background, other.background) &&
               detail::theme_color_equal(surface, other.surface) &&
               detail::theme_color_equal(text, other.text) &&
               detail::theme_color_equal(muted_text, other.muted_text) &&
               detail::theme_color_equal(border, other.border) &&
               detail::theme_color_equal(accent, other.accent) &&
               detail::theme_color_equal(disabled, other.disabled) &&
               detail::theme_color_equal(selection, other.selection) &&
               detail::theme_color_equal(focus, other.focus) &&
               detail::theme_color_equal(control_background, other.control_background) &&
               detail::theme_color_equal(control_hover, other.control_hover) &&
               detail::theme_color_equal(active_highlight, other.active_highlight) &&
               detail::theme_color_equal(track, other.track);
    }
};

struct ThemeTypography {
    std::string family;
    std::vector<std::string> fallback_families;
    float base_size{14.0f};
    float control_size{13.0f};
    float label_size{14.0f};
    FontWeight base_weight{FontWeight::Regular};
    FontWeight control_weight{FontWeight::Regular};
    FontWeight label_weight{FontWeight::Regular};
    FontSlant slant{FontSlant::Upright};

    [[nodiscard]] bool operator==(const ThemeTypography&) const = default;
};

struct ThemeSpacing {
    float xs{4.0f};
    float sm{8.0f};
    float medium{12.0f};
    float large{16.0f};
    float xl{24.0f};

    [[nodiscard]] constexpr bool operator==(const ThemeSpacing&) const noexcept = default;
};

struct ThemeRadii {
    float sm{4.0f};
    float medium{8.0f};
    float large{12.0f};

    [[nodiscard]] constexpr bool operator==(const ThemeRadii&) const noexcept = default;
};

struct ThemeControlMetrics {
    // These three fields participate directly in current standard-control
    // measurement and therefore require layout invalidation when changed.
    float minimum_width{88.0f};
    float control_height{40.0f};
    float minimum_hit_target{32.0f};

    // These metrics affect painting inside already measured bounds in T037.
    // T038 may later move per-widget geometry-affecting style data into the
    // corresponding typed widget style contract.
    float thumb_diameter{14.0f};
    float track_thickness{4.0f};
    float border_width{1.0f};
    float focus_ring_width{2.0f};

    [[nodiscard]] constexpr bool operator==(const ThemeControlMetrics&) const noexcept = default;
};

struct Theme {
    ThemePalette palette{};
    ThemeTypography typography{};
    ThemeSpacing spacing{};
    ThemeRadii radii{};
    ThemeControlMetrics controls{};

    [[nodiscard]] bool operator==(const Theme&) const = default;
};

/// Classification used by UI::set_theme(). Palette, radii and paint-only
/// control metrics dirty paint. Typography, spacing and the control metrics
/// that participate in measurement require layout + paint.
enum class ThemeInvalidation {
    None,
    Paint,
    Layout,
};

[[nodiscard]] inline Theme default_theme() {
    return Theme{};
}

namespace detail {

[[nodiscard]] constexpr bool layout_control_metrics_equal(
    const ThemeControlMetrics& a,
    const ThemeControlMetrics& b) noexcept {
    return a.minimum_width == b.minimum_width &&
           a.control_height == b.control_height &&
           a.minimum_hit_target == b.minimum_hit_target;
}

} // namespace detail

[[nodiscard]] inline ThemeInvalidation classify_theme_change(
    const Theme& previous,
    const Theme& next) {
    if (previous == next) return ThemeInvalidation::None;
    if (!(previous.typography == next.typography) ||
        !(previous.spacing == next.spacing) ||
        !detail::layout_control_metrics_equal(previous.controls, next.controls)) {
        return ThemeInvalidation::Layout;
    }
    return ThemeInvalidation::Paint;
}

} // namespace ui
