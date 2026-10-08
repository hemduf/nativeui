#pragma once

#include <nativeui/geometry.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ui::detail {

// Per-renderer conservative union of logical invalidations. Reject overflow to
// trigger the caller's full-scene repaint, without losing accepted damage.
class PendingSceneDamage final {
public:
    [[nodiscard]] bool retain(Rect rect) noexcept {
        if (!std::isfinite(rect.x) || !std::isfinite(rect.y) ||
            !std::isfinite(rect.w) || !std::isfinite(rect.h) ||
            !(rect.w > 0.0f) || !(rect.h > 0.0f)) {
            return false;
        }
        const double right = static_cast<double>(rect.x) + rect.w;
        const double bottom = static_cast<double>(rect.y) + rect.h;
        if (!std::isfinite(right) || !std::isfinite(bottom)) return false;

        if (!valid_) {
            rect_ = rect;
            valid_ = true;
            return true;
        }

        const double left_union = (std::min)(
            static_cast<double>(rect_.x), static_cast<double>(rect.x));
        const double top_union = (std::min)(
            static_cast<double>(rect_.y), static_cast<double>(rect.y));
        const double right_union = (std::max)(
            static_cast<double>(rect_.x) + rect_.w, right);
        const double bottom_union = (std::max)(
            static_cast<double>(rect_.y) + rect_.h, bottom);
        constexpr double max_float =
            static_cast<double>(std::numeric_limits<float>::max());
        if (left_union < -max_float || top_union < -max_float ||
            right_union > max_float || bottom_union > max_float ||
            right_union - left_union > max_float ||
            bottom_union - top_union > max_float) {
            return false;
        }
        const auto lower = [](double value) noexcept {
            float result = static_cast<float>(value);
            if (static_cast<double>(result) > value) {
                result = std::nextafter(result,
                                        -std::numeric_limits<float>::infinity());
            }
            return result;
        };
        const auto upper = [](double value) noexcept {
            float result = static_cast<float>(value);
            if (static_cast<double>(result) < value) {
                result = std::nextafter(result,
                                        std::numeric_limits<float>::infinity());
            }
            return result;
        };
        const float x = lower(left_union);
        const float y = lower(top_union);
        const float right_edge = upper(right_union);
        const float bottom_edge = upper(bottom_union);
        const double width_extent = static_cast<double>(right_edge) - x;
        const double height_extent = static_cast<double>(bottom_edge) - y;
        if (width_extent > max_float || height_extent > max_float) return false;
        const float width = upper(width_extent);
        const float height = upper(height_extent);
        if (!std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(width) || !std::isfinite(height)) {
            return false;
        }
        rect_ = Rect{x, y, width, height};
        return true;
    }

    [[nodiscard]] bool covers(Rect inner) const noexcept {
        if (!valid_) return false;
        const Rect outer = rect_;
        const double outer_right = static_cast<double>(outer.x) + outer.w;
        const double outer_bottom = static_cast<double>(outer.y) + outer.h;
        const double inner_right = static_cast<double>(inner.x) + inner.w;
        const double inner_bottom = static_cast<double>(inner.y) + inner.h;
        return std::isfinite(outer_right) && std::isfinite(outer_bottom) &&
               std::isfinite(inner_right) && std::isfinite(inner_bottom) &&
               outer.x <= inner.x && outer.y <= inner.y &&
               outer_right >= inner_right && outer_bottom >= inner_bottom;
    }

    void clear() noexcept { rect_ = {}; valid_ = false; }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] Rect rect() const noexcept { return rect_; }

private:
    Rect rect_{};
    bool valid_{};
};

} // namespace ui::detail
