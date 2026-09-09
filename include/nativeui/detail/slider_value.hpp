#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ui::detail {

// Shared immutable numeric domain for Slider and RangeSlider. This helper owns
// only mapping/normalization rules; widget interaction and bound State lifetime
// remain outside it.
class SliderDomain final {
public:
    SliderDomain(float minimum, float maximum, float step)
        : minimum_(minimum), maximum_(maximum), step_(step) {
        if (!std::isfinite(minimum_) || !std::isfinite(maximum_) || minimum_ >= maximum_) {
            throw std::invalid_argument("Slider range must be finite with minimum < maximum");
        }
        if (!std::isfinite(step_) || step_ < 0.0f) {
            throw std::invalid_argument("Slider step must be zero or a positive finite value");
        }
    }

    [[nodiscard]] float minimum() const noexcept { return minimum_; }
    [[nodiscard]] float maximum() const noexcept { return maximum_; }
    [[nodiscard]] float step() const noexcept { return step_; }

    // Normalize a value written by user interaction: finite fallback first,
    // then the ticket's exact quantize-to-grid formula, then range clamp.
    [[nodiscard]] float normalize(float value) const noexcept {
        if (!std::isfinite(value)) return minimum_;

        float normalized = value;
        if (step_ > 0.0f) {
            const float steps = std::round((value - minimum_) / step_);
            normalized = minimum_ + steps * step_;
        }
        return std::clamp(normalized, minimum_, maximum_);
    }

    // External State values are never rewritten or silently snapped merely for
    // mount/paint. Rendering/hit-testing only applies the safe fallback + clamp.
    [[nodiscard]] float effective_external(float value) const noexcept {
        if (!std::isfinite(value)) return minimum_;
        return std::clamp(value, minimum_, maximum_);
    }

    [[nodiscard]] float fraction(float value) const noexcept {
        const float effective = effective_external(value);
        return (effective - minimum_) / (maximum_ - minimum_);
    }

    [[nodiscard]] float value_from_fraction(float fraction) const noexcept {
        if (!std::isfinite(fraction)) return minimum_;
        const float clamped = std::clamp(fraction, 0.0f, 1.0f);
        return normalize(minimum_ + clamped * (maximum_ - minimum_));
    }

    [[nodiscard]] float keyboard_increment(bool shift) const noexcept {
        if (step_ > 0.0f) return step_;
        return (maximum_ - minimum_) / (shift ? 1000.0f : 100.0f);
    }

private:
    float minimum_{};
    float maximum_{};
    float step_{};
};

} // namespace ui::detail
