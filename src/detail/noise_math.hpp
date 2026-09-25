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

/// Frozen T089 gradient table. S is the mathematical 1/sqrt(2) with the exact
/// decimal literal shared by the C++ reference and the SkSL kernel.
struct NoiseGradient {
    double x;
    double y;
};

[[nodiscard]] constexpr NoiseGradient noise_gradient(
    std::uint32_t gradient_index) noexcept {
    constexpr double s = 0.707106781186547524400844362104849;
    constexpr NoiseGradient table[8] = {
        {1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0},
        {s, s}, {-s, s}, {s, -s}, {-s, -s},
    };
    return table[gradient_index & 7u];
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

[[nodiscard]] inline double perlin_noise_reference(std::uint32_t seed,
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
    const auto corner = [seed](std::int32_t a, std::int32_t b,
                               double dx, double dy) noexcept {
        const NoiseGradient g = noise_gradient(noise_hash2(seed, a, b));
        return g.x * dx + g.y * dy;
    };
    const double d00 = corner(lx, ly, fx, fy);
    const double d10 = corner(lx + 1, ly, fx - 1.0, fy);
    const double d01 = corner(lx, ly + 1, fx, fy - 1.0);
    const double d11 = corner(lx + 1, ly + 1, fx - 1.0, fy - 1.0);
    const double ux = fade(fx);
    const double uy = fade(fy);
    const double r0 = d00 + (d10 - d00) * ux;
    const double r1 = d01 + (d11 - d01) * ux;
    const double raw = r0 + (r1 - r0) * uy;
    const double value = 0.5 + raw / 2.8284271247461900976033774484194;
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

} // namespace ui::detail
