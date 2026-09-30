#pragma once

#include <nativeui/material.hpp>

#include <algorithm>
#include <cmath>
#include <variant>

namespace ui::detail {

struct MaterialAccess final {
    [[nodiscard]] static const Color* solid_color(
        const Brush& brush) noexcept {
        return std::get_if<Color>(&brush.value_);
    }

    [[nodiscard]] static Color sanitize_albedo(Color value) noexcept {
        return {
            sanitize_unit(value.r),
            sanitize_unit(value.g),
            sanitize_unit(value.b),
            sanitize_unit(value.a)};
    }

    [[nodiscard]] static float sanitize_roughness(float value) noexcept {
        if (!std::isfinite(value)) return 0.5f;
        return std::clamp(value, 0.045f, 1.0f);
    }

    [[nodiscard]] static float sanitize_metallic(float value) noexcept {
        return sanitize_unit(value);
    }

    [[nodiscard]] static float sanitize_emissive_channel(
        float value) noexcept {
        return sanitize_unit(value);
    }

    [[nodiscard]] static float sanitize_emissive_intensity(
        float value) noexcept {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(value, 0.0f, 64.0f);
    }

private:
    [[nodiscard]] static float sanitize_unit(float value) noexcept {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(value, 0.0f, 1.0f);
    }
};

} // namespace ui::detail
