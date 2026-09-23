#pragma once

#include <nativeui/geometry.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace ui::detail {

struct SceneExtent final {
    int width{};
    int height{};
};

inline constexpr std::uint64_t kMaxSceneBytes = 128ULL * 1024ULL * 1024ULL;

// RGBA8, single sample. At most one old and one replacement scene may coexist,
// bounding the two installed scene allocations to 256 MiB per view. Skia's
// internal cache and the Pugl-owned backbuffer are separate allocations.
[[nodiscard]] inline std::optional<SceneExtent> validate_scene_extent(
    Size physical,
    float scale,
    int backend_limit) noexcept {
    if (!std::isfinite(physical.w) || !std::isfinite(physical.h) ||
        physical.w <= 0.0f || physical.h <= 0.0f ||
        !std::isfinite(scale) || scale <= 0.0f || backend_limit <= 0) {
        return std::nullopt;
    }
    const double rounded_width = std::floor(static_cast<double>(physical.w) + 0.5);
    const double rounded_height = std::floor(static_cast<double>(physical.h) + 0.5);
    const auto maximum = static_cast<double>(std::numeric_limits<int>::max());
    if (rounded_width < 1.0 || rounded_height < 1.0 ||
        rounded_width > maximum || rounded_height > maximum ||
        rounded_width > backend_limit || rounded_height > backend_limit) {
        return std::nullopt;
    }
    const auto width = static_cast<int>(rounded_width);
    const auto height = static_cast<int>(rounded_height);
    if (static_cast<std::uint64_t>(width) >
        kMaxSceneBytes / (4ULL * static_cast<std::uint64_t>(height))) {
        return std::nullopt;
    }
    return SceneExtent{width, height};
}

} // namespace ui::detail
