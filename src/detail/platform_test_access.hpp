#pragma once

#include <nativeui/window.hpp>

#include <cstdint>
#include <optional>

namespace ui::detail {

struct PlatformReadbackPixel final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

struct PlatformTestAccess final {
    [[nodiscard]] static bool request_gpu_readback(
        StandaloneWindow& window,
        Point logical_point) noexcept;

    [[nodiscard]] static std::optional<PlatformReadbackPixel>
    take_gpu_readback(StandaloneWindow& window) noexcept;
};

} // namespace ui::detail
