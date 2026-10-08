#pragma once

#include <nativeui/input.hpp>

namespace ui::detail {

// Pugl normalizes scrolling to "positive dy = scroll up" on every backend:
// src/mac.m maps `dy > 0` to PUGL_SCROLL_UP (precise two-finger deltas arrive
// divided by 20), src/win.c maps a wheel-up notch to dy = +1, src/x11.c maps
// Button4 to dy = +1.0, and src/emscripten_events.c negates the browser's
// down-positive deltaY. The editor convention this event serves (Figma-style
// ctrl/cmd zoom) treats scroll up as zoom in, so one sign serves every
// backend; a per-platform sign branch would invert the gesture on macOS.
//
// kMagnifyScrollFactor converts one Pugl scroll unit into the continuous
// relative scale factor of InputType::Magnify. A ctrl/cmd wheel notch is
// |dy| = 1.0 on Win/X11, i.e. one notch = 10% zoom (the browser/editor
// convention); macOS precise deltas arrive as fractions of a unit per event,
// giving the continuous feel the trackpad produces. 0.1 is the value qualified
// on the macOS normalized path; retune only with recorded A/B evidence.
inline constexpr float kMagnifyScrollFactor = 0.1F;

/// Normalization of one Pugl scroll event into its nativeui delivery.
struct PuglScrollTranslation {
    /// PointerWheel for plain scrolling, Magnify for the zoom gesture.
    InputType type{InputType::PointerWheel};
    /// Relative scale factor, only meaningful when type is Magnify.
    float magnification{0.0F};
};

/// Ctrl/cmd + scroll means zoom on every platform: Windows synthesizes
/// ctrl+wheel for the trackpad pinch, browsers send ctrlKey wheel events for
/// the pinch gesture, and explicit modifier+scroll means zoom. After this
/// normalization PointerWheel never carries ctrl or gui.
[[nodiscard]] constexpr PuglScrollTranslation translate_pugl_scroll(
    bool ctrl, bool gui, double dy) noexcept {
    if (ctrl || gui) {
        return {InputType::Magnify,
                static_cast<float>(dy * kMagnifyScrollFactor)};
    }
    return {InputType::PointerWheel, 0.0F};
}

} // namespace ui::detail
