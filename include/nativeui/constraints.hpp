#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ui {

inline constexpr float kUnboundedExtent = std::numeric_limits<float>::infinity();

/// Box constraints expressed in logical pixels. `max` may be +infinity to
/// represent an unbounded axis; all other invalid/negative values are
/// normalized so layout never propagates NaN or negative extents.
struct Constraints {
    Size min{};
    Size max{kUnboundedExtent, kUnboundedExtent};

    Constraints() = default;
    Constraints(Size minimum, Size maximum)
        : min{sanitize_min(minimum.w), sanitize_min(minimum.h)},
          max{sanitize_max(maximum.w, min.w), sanitize_max(maximum.h, min.h)} {}

    [[nodiscard]] static Constraints unbounded() noexcept { return {}; }
    [[nodiscard]] static Constraints loose(Size maximum) noexcept {
        return Constraints{{0.0f, 0.0f}, maximum};
    }
    [[nodiscard]] static Constraints tight(Size size) noexcept {
        const Size clean{sanitize_min(size.w), sanitize_min(size.h)};
        return Constraints{clean, clean};
    }

    [[nodiscard]] bool bounded_width() const noexcept {
        return std::isfinite(max.w) && max.w >= 0.0f;
    }
    [[nodiscard]] bool bounded_height() const noexcept {
        return std::isfinite(max.h) && max.h >= 0.0f;
    }

    [[nodiscard]] Constraints loosen() const noexcept {
        return Constraints{{0.0f, 0.0f}, max};
    }

    [[nodiscard]] Constraints inset(float horizontal, float vertical) const noexcept {
        const Constraints normalized{min, max};
        horizontal = sanitize_min(horizontal);
        vertical = sanitize_min(vertical);
        const float dx = horizontal * 2.0f;
        const float dy = vertical * 2.0f;
        return Constraints{
            {std::max(0.0f, normalized.min.w - dx),
             std::max(0.0f, normalized.min.h - dy)},
            {subtract_if_bounded(normalized.max.w, dx),
             subtract_if_bounded(normalized.max.h, dy)}};
    }

    [[nodiscard]] Size constrain(Size size) const noexcept {
        const float min_w = sanitize_min(min.w);
        const float min_h = sanitize_min(min.h);
        const float max_w = sanitize_max(max.w, min_w);
        const float max_h = sanitize_max(max.h, min_h);
        return Size{constrain_extent(size.w, min_w, max_w),
                    constrain_extent(size.h, min_h, max_h)};
    }

private:
    [[nodiscard]] static float sanitize_min(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    [[nodiscard]] static float sanitize_max(float value, float minimum) noexcept {
        if (std::isinf(value) && value > 0.0f) return value;
        if (!std::isfinite(value)) return minimum;
        return std::max(minimum, std::max(0.0f, value));
    }

    [[nodiscard]] static float subtract_if_bounded(float value, float amount) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value - amount) : value;
    }

    [[nodiscard]] static float constrain_extent(float value, float minimum, float maximum) noexcept {
        if (!std::isfinite(value)) {
            value = (std::isinf(value) && value > 0.0f && std::isfinite(maximum))
                        ? maximum
                        : minimum;
        }
        return std::clamp(std::max(0.0f, value), minimum, maximum);
    }
};

} // namespace ui
