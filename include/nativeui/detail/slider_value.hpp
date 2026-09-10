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
    // then the ticket's exact quantize-to-grid formula, then range clamp. Use
    // double intermediates because two valid finite float endpoints can have a
    // span that is not itself representable as float.
    [[nodiscard]] float normalize(double value) const noexcept {
        if (!std::isfinite(value)) return minimum_;

        double normalized = value;
        if (step_ > 0.0f) {
            const double steps = std::round(
                (value - static_cast<double>(minimum_)) / static_cast<double>(step_));
            normalized = static_cast<double>(minimum_) +
                         steps * static_cast<double>(step_);
        }
        normalized = std::clamp(
            normalized,
            static_cast<double>(minimum_),
            static_cast<double>(maximum_));
        return static_cast<float>(normalized);
    }

    // External State values are never rewritten or silently snapped merely for
    // mount/paint. Rendering/hit-testing only applies the safe fallback + clamp.
    [[nodiscard]] float effective_external(float value) const noexcept {
        if (!std::isfinite(value)) return minimum_;
        return std::clamp(value, minimum_, maximum_);
    }

    [[nodiscard]] float fraction(float value) const noexcept {
        const double effective = static_cast<double>(effective_external(value));
        const double minimum = static_cast<double>(minimum_);
        const double span = static_cast<double>(maximum_) - minimum;
        const double fraction = (effective - minimum) / span;
        return static_cast<float>(std::clamp(fraction, 0.0, 1.0));
    }

    // Continuous pointer-domain value before step quantization. RangeSlider
    // uses this for nearest-thumb hit selection so the step grid cannot create
    // an artificial tie. The selected thumb's eventual write still goes
    // through normalize().
    [[nodiscard]] float raw_value_from_fraction(float fraction) const noexcept {
        if (!std::isfinite(fraction)) return minimum_;
        const double clamped = std::clamp(static_cast<double>(fraction), 0.0, 1.0);
        const double minimum = static_cast<double>(minimum_);
        const double maximum = static_cast<double>(maximum_);
        const double value = minimum + clamped * (maximum - minimum);
        return static_cast<float>(std::clamp(value, minimum, maximum));
    }

    [[nodiscard]] float value_from_fraction(float fraction) const noexcept {
        return normalize(static_cast<double>(raw_value_from_fraction(fraction)));
    }

    [[nodiscard]] double keyboard_increment(bool shift) const noexcept {
        if (step_ > 0.0f) return static_cast<double>(step_);
        const double span = static_cast<double>(maximum_) - static_cast<double>(minimum_);
        return span / (shift ? 1000.0 : 100.0);
    }

private:
    float minimum_{};
    float maximum_{};
    float step_{};
};

} // namespace ui::detail
