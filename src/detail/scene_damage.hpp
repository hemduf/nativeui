#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace ui::detail {

struct DeviceDamage final {
    int left{};
    int top{};
    int right{};
    int bottom{};
    Rect logical_query{};
    bool covers_scene{};
};

[[nodiscard]] inline std::optional<DeviceDamage> map_device_damage(
    Rect logical_damage,
    float scale_factor,
    int physical_width,
    int physical_height) noexcept {
    if (!std::isfinite(logical_damage.x) ||
        !std::isfinite(logical_damage.y) ||
        !std::isfinite(logical_damage.w) ||
        !std::isfinite(logical_damage.h) ||
        !(logical_damage.w > 0.0f) || !(logical_damage.h > 0.0f) ||
        !std::isfinite(scale_factor) || !(scale_factor > 0.0f) ||
        physical_width <= 0 || physical_height <= 0) {
        return std::nullopt;
    }

    constexpr double kMaxExactFloatInteger = 16777216.0;
    constexpr double kAntialiasGuardPixels = 2.0;
    const double right = static_cast<double>(logical_damage.x) +
                         static_cast<double>(logical_damage.w);
    const double bottom = static_cast<double>(logical_damage.y) +
                          static_cast<double>(logical_damage.h);
    const double device_left = static_cast<double>(logical_damage.x) * scale_factor;
    const double device_top = static_cast<double>(logical_damage.y) * scale_factor;
    const double device_right = right * scale_factor;
    const double device_bottom = bottom * scale_factor;
    if (!std::isfinite(right) || !std::isfinite(bottom) ||
        !std::isfinite(device_left) || !std::isfinite(device_top) ||
        !std::isfinite(device_right) || !std::isfinite(device_bottom) ||
        device_left < -kMaxExactFloatInteger ||
        device_top < -kMaxExactFloatInteger ||
        device_right > kMaxExactFloatInteger ||
        device_bottom > kMaxExactFloatInteger ||
        physical_width > kMaxExactFloatInteger ||
        physical_height > kMaxExactFloatInteger) {
        return std::nullopt;
    }

    const double guarded_left = std::floor(device_left) - kAntialiasGuardPixels;
    const double guarded_top = std::floor(device_top) - kAntialiasGuardPixels;
    const double guarded_right = std::ceil(device_right) + kAntialiasGuardPixels;
    const double guarded_bottom = std::ceil(device_bottom) + kAntialiasGuardPixels;
    if (!std::isfinite(guarded_left) || !std::isfinite(guarded_top) ||
        !std::isfinite(guarded_right) || !std::isfinite(guarded_bottom)) {
        return std::nullopt;
    }

    const int left = static_cast<int>(std::clamp(
        guarded_left, 0.0, static_cast<double>(physical_width)));
    const int top = static_cast<int>(std::clamp(
        guarded_top, 0.0, static_cast<double>(physical_height)));
    const int right_edge = static_cast<int>(std::clamp(
        guarded_right, 0.0, static_cast<double>(physical_width)));
    const int bottom_edge = static_cast<int>(std::clamp(
        guarded_bottom, 0.0, static_cast<double>(physical_height)));
    if (left >= right_edge || top >= bottom_edge) return std::nullopt;

    const auto round_down = [](double value) noexcept {
        float result = static_cast<float>(value);
        if (static_cast<double>(result) > value) {
            result = std::nextafter(result,
                                    -std::numeric_limits<float>::infinity());
        }
        return result;
    };
    const auto round_up = [](double value) noexcept {
        float result = static_cast<float>(value);
        if (static_cast<double>(result) < value) {
            result = std::nextafter(result,
                                    std::numeric_limits<float>::infinity());
        }
        return result;
    };
    const double logical_left = static_cast<double>(left) / scale_factor;
    const double logical_top = static_cast<double>(top) / scale_factor;
    const double logical_right = static_cast<double>(right_edge) / scale_factor;
    const double logical_bottom = static_cast<double>(bottom_edge) / scale_factor;
    constexpr double max_float =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(logical_left) || !std::isfinite(logical_top) ||
        !std::isfinite(logical_right) || !std::isfinite(logical_bottom) ||
        logical_left < -max_float || logical_top < -max_float ||
        logical_right > max_float || logical_bottom > max_float) {
        return std::nullopt;
    }
    const float query_left = round_down(logical_left);
    const float query_top = round_down(logical_top);
    const float query_right = round_up(logical_right);
    const float query_bottom = round_up(logical_bottom);
    const double query_width_extent = static_cast<double>(query_right) - query_left;
    const double query_height_extent = static_cast<double>(query_bottom) - query_top;
    if (query_width_extent > max_float || query_height_extent > max_float) {
        return std::nullopt;
    }
    const float query_width = round_up(query_width_extent);
    const float query_height = round_up(query_height_extent);
    if (!std::isfinite(query_left) || !std::isfinite(query_top) ||
        !std::isfinite(query_width) || !std::isfinite(query_height) ||
        !(query_width > 0.0f) || !(query_height > 0.0f)) {
        return std::nullopt;
    }

    return DeviceDamage{
        left,
        top,
        right_edge,
        bottom_edge,
        Rect{query_left, query_top, query_width, query_height},
        left == 0 && top == 0 && right_edge == physical_width &&
            bottom_edge == physical_height};
}

} // namespace ui::detail
