#pragma once

#include <nativeui/geometry.hpp>

#include <cmath>
#include <optional>

namespace ui::detail {

/// AppKit accessibility frame expressed in Cocoa screen points.
///
/// Native semantic publications intentionally retain geometry in Pugl's
/// physical, top-down screen coordinate system. AppKit accessibility frames use
/// Cocoa screen points with a bottom-up Y axis, so the macOS bridge performs the
/// inverse coordinate-basis conversion at its final platform boundary.
struct MacOSAccessibilityScreenFrame final {
    double x{};
    double y{};
    double width{};
    double height{};
};

/// Convert one immutable physical top-down screen rectangle into the Cocoa
/// screen-point basis consumed by NSAccessibilityElement.
///
/// `main_screen_height_points` is the same main-screen reference height used by
/// the Cocoa child-to-screen origin capture. The backing scale is removed exactly
/// once; the Y axis is then mirrored exactly once. Invalid/non-finite inputs fail
/// closed instead of publishing a partially converted frame.
[[nodiscard]] inline std::optional<MacOSAccessibilityScreenFrame>
macos_accessibility_screen_frame(Rect physical_screen_bounds,
                                 float backing_scale,
                                 double main_screen_height_points) noexcept {
    const double scale = static_cast<double>(backing_scale);
    const double x = static_cast<double>(physical_screen_bounds.x);
    const double y = static_cast<double>(physical_screen_bounds.y);
    const double width = static_cast<double>(physical_screen_bounds.w);
    const double height = static_cast<double>(physical_screen_bounds.h);

    if (!std::isfinite(scale) || scale <= 0.0 ||
        !std::isfinite(main_screen_height_points) ||
        main_screen_height_points <= 0.0 ||
        !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        width < 0.0 || height < 0.0) {
        return std::nullopt;
    }

    MacOSAccessibilityScreenFrame frame{
        x / scale,
        main_screen_height_points - ((y + height) / scale),
        width / scale,
        height / scale,
    };

    if (!std::isfinite(frame.x) || !std::isfinite(frame.y) ||
        !std::isfinite(frame.width) || !std::isfinite(frame.height)) {
        return std::nullopt;
    }

    return frame;
}

} // namespace ui::detail
