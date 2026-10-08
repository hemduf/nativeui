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

/// Shared color tokens used to seed standard-widget style recipes.
///
/// Colors are ordinary Theme-owned values. This layer performs no clamping,
/// validation or color-space conversion; typed widget recipes consume the
/// tokens and Painter/rendering services interpret the resulting Color values.
struct ThemePalette {
    /// Root/background color and common contrasting foreground for active controls.
    Color background{colors::background};
    /// Raised/panel surface color used by default control recipes.
    Color surface{colors::panel};
    /// Primary readable text color.
    Color text{colors::text};
    /// Secondary, placeholder and subdued-label text color.
    Color muted_text{colors::textMuted};
    /// Default control/panel border color.
    Color border{colors::border};
    /// Primary accent/activation color.
    Color accent{colors::accent};
    /// Foreground token used by disabled-state recipes.
    Color disabled{colors::textMuted};
    /// Text/list selection highlight color.
    Color selection{colors::selection};
    /// Focus indication color.
    Color focus{colors::borderFocus};

    // Shared interaction colors used by the existing standard controls. T038
    // owns per-widget state patches; these remain global theme tokens only.
    /// Shared resting input/control fill.
    Color control_background{colors::input};
    /// Shared hover-state control color.
    Color control_hover{colors::knob};
    /// Shared active/caret-like highlight token.
    Color active_highlight{colors::caret};
    /// Shared slider/progress/scroll track token.
    Color track{colors::track};

    /// Exact component-wise equality; no epsilon or color canonicalization.
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

/// Typography tokens inherited by default widget recipes.
///
/// Font sizes are logical UI units. Family names and fallback order are copied
/// into resolved styles; constructing/comparing Theme values does not perform
/// font lookup. Values are stored verbatim.
struct ThemeTypography {
    /// Preferred font family; empty delegates to the text service default.
    std::string family;
    /// Ordered fallback families consulted after `family`.
    std::vector<std::string> fallback_families;
    /// General body/default font size in logical UI units.
    float base_size{14.0f};
    /// Standard control text size in logical UI units.
    float control_size{13.0f};
    /// Standard label text size in logical UI units.
    float label_size{14.0f};
    /// Default body text weight.
    FontWeight base_weight{FontWeight::Regular};
    /// Default control text weight.
    FontWeight control_weight{FontWeight::Regular};
    /// Default label text weight.
    FontWeight label_weight{FontWeight::Regular};
    /// Default slant shared by theme-derived text styles.
    FontSlant slant{FontSlant::Upright};

    /// Full value equality, including ordered fallback names and exact floats.
    [[nodiscard]] bool operator==(const ThemeTypography&) const = default;
};

/// Reusable spacing scale in logical UI units.
///
/// Theme stores these tokens verbatim; monotonicity and non-negative geometry
/// are not enforced here.
struct ThemeSpacing {
    /// Extra-small spacing token.
    float xs{4.0f};
    /// Small spacing token.
    float sm{8.0f};
    /// Medium spacing token.
    float medium{12.0f};
    /// Large spacing token.
    float large{16.0f};
    /// Extra-large spacing token.
    float xl{24.0f};

    /// Exact token equality.
    [[nodiscard]] constexpr bool operator==(const ThemeSpacing&) const noexcept = default;
};

/// Shared corner-radius scale in logical UI units.
/// Values are stored verbatim; individual drawing/widget paths own any
/// geometry-specific canonicalization.
struct ThemeRadii {
    /// Small corner radius.
    float sm{4.0f};
    /// Medium/default control corner radius.
    float medium{8.0f};
    /// Large corner radius.
    float large{12.0f};

    /// Exact token equality.
    [[nodiscard]] constexpr bool operator==(const ThemeRadii&) const noexcept = default;
};

/// Shared standard-control geometry tokens, in logical UI units.
///
/// `minimum_width`, `control_height` and `minimum_hit_target` participate in
/// retained measurement and therefore make a Theme replacement layout-affecting.
/// The remaining fields are classified as paint-only by `classify_theme_change`.
/// Theme storage itself does not clamp invalid geometry.
struct ThemeControlMetrics {
    // These three fields participate directly in current standard-control
    // measurement and therefore require layout invalidation when changed.
    /// Shared minimum measured width for standard controls.
    float minimum_width{88.0f};
    /// Shared measured control height.
    float control_height{40.0f};
    /// Minimum intended interactive target extent.
    float minimum_hit_target{32.0f};

    // These metrics affect painting inside already measured bounds in T037.
    // T038 may later move per-widget geometry-affecting style data into the
    // corresponding typed widget style contract.
    /// Shared slider-like thumb diameter used by paint recipes.
    float thumb_diameter{14.0f};
    /// Shared track thickness used by slider/progress-style painting.
    float track_thickness{4.0f};
    /// Default border stroke width.
    float border_width{1.0f};
    /// Default focused border/ring stroke width.
    float focus_ring_width{2.0f};

    /// Exact token equality.
    [[nodiscard]] constexpr bool operator==(const ThemeControlMetrics&) const noexcept = default;
};

/// Complete root theme value owned by a UI/retained tree.
///
/// Theme is ordinary copyable value data: it contains no backend handles,
/// observers, borrowed UI pointers or process-global state. Replacing a live
/// UI's theme remains UI/main-thread retained work.
struct Theme {
    /// Shared color tokens.
    ThemePalette palette{};
    /// Shared typography tokens.
    ThemeTypography typography{};
    /// Shared spacing scale.
    ThemeSpacing spacing{};
    /// Shared corner-radius scale.
    ThemeRadii radii{};
    /// Shared standard-control geometry/paint metrics.
    ThemeControlMetrics controls{};

    /// Full value equality across every theme family.
    [[nodiscard]] bool operator==(const Theme&) const = default;
};

/// Classification used by UI::set_theme(). Palette, radii and paint-only
/// control metrics dirty paint. Typography, spacing and the control metrics
/// that participate in measurement require layout + paint.
enum class ThemeInvalidation {
    /// Effective previous and next Theme values are identical.
    None,
    /// Geometry/measurement is unchanged; repaint is sufficient.
    Paint,
    /// Measurement can change; retained layout and paint must be refreshed.
    Layout,
};

/// Return the canonical default Theme value by value; no shared singleton is exposed.
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

/// Classify the minimum retained invalidation required by a Theme replacement.
///
/// Typography, spacing or measurement-affecting control metric differences
/// return `Layout`; any other unequal Theme returns `Paint`; exact equality
/// returns `None`. This is pure value comparison with no callbacks or UI mutation.
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
