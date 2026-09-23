#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ui::detail {

[[nodiscard]] constexpr std::uint32_t noise_hash2(std::uint32_t seed,
                                                    std::int32_t ix,
                                                    std::int32_t iy) noexcept {
    std::uint32_t h = seed;
    h ^= std::uint32_t(ix) * 0x9E3779B9u;
    h = (h << 17) | (h >> 15);
    h ^= std::uint32_t(iy) * 0x85EBCA6Bu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

[[nodiscard]] constexpr float noise_u24(std::uint32_t hash) noexcept {
    return static_cast<float>(hash >> 8) * (1.0f / 16777216.0f);
}

[[nodiscard]] inline double value_noise_reference(std::uint32_t seed,
                                                   double feature_size,
                                                   double x,
                                                   double y) noexcept {
    const double nx = x / feature_size;
    const double ny = y / feature_size;
    constexpr double lower = -2147483648.0;
    constexpr double upper = 2147483647.0;
    if (!(nx >= lower && nx < upper && ny >= lower && ny < upper)) return 0.5;

    const double ix = std::floor(nx);
    const double iy = std::floor(ny);
    const auto lx = static_cast<std::int32_t>(ix);
    const auto ly = static_cast<std::int32_t>(iy);
    const double fx = nx - ix;
    const double fy = ny - iy;
    const auto fade = [](double t) noexcept {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    };
    const auto lattice = [seed](std::int32_t a, std::int32_t b) noexcept {
        return static_cast<double>(noise_hash2(seed, a, b) >> 8) /
               16777216.0;
    };
    const double v00 = lattice(lx, ly);
    const double v10 = lattice(lx + 1, ly);
    const double v01 = lattice(lx, ly + 1);
    const double v11 = lattice(lx + 1, ly + 1);
    const double ux = fade(fx);
    const double uy = fade(fy);
    const double a = v00 + (v10 - v00) * ux;
    const double b = v01 + (v11 - v01) * ux;
    return a + (b - a) * uy;
}

} // namespace ui::detail
