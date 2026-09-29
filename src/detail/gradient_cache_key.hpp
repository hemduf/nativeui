#pragma once

#include <nativeui/paint_style.hpp>

#include "include/core/SkColor.h"
#include "include/core/SkPoint.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace ui::detail {

struct LinearGradientCacheLookup final {
    const LinearGradient* gradient{};
};

struct RadialGradientCacheLookup final {
    const RadialGradient* gradient{};
};

struct LinearGradientCacheKey final {
    Point start{};
    Point end{};
    std::vector<GradientStop> stops;

    explicit LinearGradientCacheKey(const LinearGradient& gradient)
        : start(gradient.start()),
          end(gradient.end()),
          stops(gradient.stops()) {}
};

struct RadialGradientCacheKey final {
    Point center{};
    float radius{};
    std::vector<GradientStop> stops;

    explicit RadialGradientCacheKey(const RadialGradient& gradient)
        : center(gradient.center()),
          radius(gradient.radius()),
          stops(gradient.stops()) {}
};

namespace gradient_cache_detail {

template <class T>
inline void hash_combine(std::size_t& seed, const T& value) noexcept {
    const auto hash = std::hash<T>{}(value);
    seed ^= hash + static_cast<std::size_t>(0x9e3779b9U) +
            (seed << 6U) + (seed >> 2U);
}

inline void hash_float(std::size_t& seed, float value) noexcept {
    hash_combine(seed, value == 0.0f ? 0.0f : value);
}

inline void hash_color(std::size_t& seed, Color color) noexcept {
    hash_float(seed, color.r);
    hash_float(seed, color.g);
    hash_float(seed, color.b);
    hash_float(seed, color.a);
}

inline void hash_stops(
    std::size_t& seed,
    std::span<const GradientStop> stops) noexcept {
    hash_combine(seed, stops.size());
    for (const auto& stop : stops) {
        hash_float(seed, stop.offset);
        hash_color(seed, stop.color);
    }
}

[[nodiscard]] inline std::size_t linear_hash(
    Point start,
    Point end,
    std::span<const GradientStop> stops) noexcept {
    std::size_t seed{};
    hash_float(seed, start.x);
    hash_float(seed, start.y);
    hash_float(seed, end.x);
    hash_float(seed, end.y);
    hash_stops(seed, stops);
    return seed;
}

[[nodiscard]] inline std::size_t radial_hash(
    Point center,
    float radius,
    std::span<const GradientStop> stops) noexcept {
    std::size_t seed{};
    hash_float(seed, center.x);
    hash_float(seed, center.y);
    hash_float(seed, radius);
    hash_stops(seed, stops);
    return seed;
}

[[nodiscard]] inline bool same_color(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

[[nodiscard]] inline bool same_stops(
    std::span<const GradientStop> a,
    std::span<const GradientStop> b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (a[index].offset != b[index].offset ||
            !same_color(a[index].color, b[index].color)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool finite_color(Color color) noexcept {
    return std::isfinite(color.r) &&
           std::isfinite(color.g) &&
           std::isfinite(color.b) &&
           std::isfinite(color.a);
}

[[nodiscard]] inline bool valid_stops(
    std::span<const GradientStop> stops) noexcept {
    if (stops.size() < 2U) return false;
    float previous = -1.0f;
    for (const auto& stop : stops) {
        if (!std::isfinite(stop.offset) ||
            stop.offset < 0.0f ||
            stop.offset > 1.0f ||
            stop.offset <= previous) {
            return false;
        }
        previous = stop.offset;
    }
    return true;
}

[[nodiscard]] inline bool cacheable_stops(
    std::span<const GradientStop> stops) noexcept {
    if (!valid_stops(stops)) return false;
    for (const auto& stop : stops) {
        if (!finite_color(stop.color)) return false;
    }
    return true;
}

[[nodiscard]] inline bool same_linear(
    const LinearGradientCacheKey& key,
    const LinearGradient& gradient) noexcept {
    const auto start = gradient.start();
    const auto end = gradient.end();
    return key.start.x == start.x &&
           key.start.y == start.y &&
           key.end.x == end.x &&
           key.end.y == end.y &&
           same_stops(key.stops, gradient.stops());
}

[[nodiscard]] inline bool same_radial(
    const RadialGradientCacheKey& key,
    const RadialGradient& gradient) noexcept {
    const auto center = gradient.center();
    return key.center.x == center.x &&
           key.center.y == center.y &&
           key.radius == gradient.radius() &&
           same_stops(key.stops, gradient.stops());
}

[[nodiscard]] inline SkColor4f sk_color(Color color) noexcept {
    return SkColor4f{color.r, color.g, color.b, color.a};
}

template <class Factory>
[[nodiscard]] sk_sp<SkShader> materialize(
    std::span<const GradientStop> stops,
    Factory&& factory) {
    if (!valid_stops(stops)) return {};

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    colors.reserve(stops.size());
    positions.reserve(stops.size());
    for (const auto& stop : stops) {
        colors.push_back(sk_color(stop.color));
        positions.push_back(stop.offset);
    }

    const SkGradient gradient{
        {{colors.data(), colors.size()},
         {positions.data(), positions.size()},
         SkTileMode::kClamp},
        {}};
    return std::forward<Factory>(factory)(gradient);
}

} // namespace gradient_cache_detail

struct LinearGradientCacheKeyHash final {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(
        const LinearGradientCacheKey& key) const noexcept {
        return gradient_cache_detail::linear_hash(
            key.start, key.end, key.stops);
    }

    [[nodiscard]] std::size_t operator()(
        LinearGradientCacheLookup lookup) const noexcept {
        if (!lookup.gradient) return 0U;
        return gradient_cache_detail::linear_hash(
            lookup.gradient->start(),
            lookup.gradient->end(),
            lookup.gradient->stops());
    }
};

struct RadialGradientCacheKeyHash final {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(
        const RadialGradientCacheKey& key) const noexcept {
        return gradient_cache_detail::radial_hash(
            key.center, key.radius, key.stops);
    }

    [[nodiscard]] std::size_t operator()(
        RadialGradientCacheLookup lookup) const noexcept {
        if (!lookup.gradient) return 0U;
        return gradient_cache_detail::radial_hash(
            lookup.gradient->center(),
            lookup.gradient->radius(),
            lookup.gradient->stops());
    }
};

struct LinearGradientCacheKeyEqual final {
    using is_transparent = void;

    [[nodiscard]] bool operator()(
        const LinearGradientCacheKey& a,
        const LinearGradientCacheKey& b) const noexcept {
        return a.start.x == b.start.x &&
               a.start.y == b.start.y &&
               a.end.x == b.end.x &&
               a.end.y == b.end.y &&
               gradient_cache_detail::same_stops(a.stops, b.stops);
    }

    [[nodiscard]] bool operator()(
        const LinearGradientCacheKey& key,
        LinearGradientCacheLookup lookup) const noexcept {
        return lookup.gradient &&
               gradient_cache_detail::same_linear(key, *lookup.gradient);
    }

    [[nodiscard]] bool operator()(
        LinearGradientCacheLookup lookup,
        const LinearGradientCacheKey& key) const noexcept {
        return (*this)(key, lookup);
    }
};

struct RadialGradientCacheKeyEqual final {
    using is_transparent = void;

    [[nodiscard]] bool operator()(
        const RadialGradientCacheKey& a,
        const RadialGradientCacheKey& b) const noexcept {
        return a.center.x == b.center.x &&
               a.center.y == b.center.y &&
               a.radius == b.radius &&
               gradient_cache_detail::same_stops(a.stops, b.stops);
    }

    [[nodiscard]] bool operator()(
        const RadialGradientCacheKey& key,
        RadialGradientCacheLookup lookup) const noexcept {
        return lookup.gradient &&
               gradient_cache_detail::same_radial(key, *lookup.gradient);
    }

    [[nodiscard]] bool operator()(
        RadialGradientCacheLookup lookup,
        const RadialGradientCacheKey& key) const noexcept {
        return (*this)(key, lookup);
    }
};

struct GradientCacheAccess final {
    [[nodiscard]] static bool cacheable(
        const LinearGradient& gradient) noexcept {
        const auto start = gradient.start();
        const auto end = gradient.end();
        return std::isfinite(start.x) &&
               std::isfinite(start.y) &&
               std::isfinite(end.x) &&
               std::isfinite(end.y) &&
               gradient_cache_detail::cacheable_stops(gradient.stops());
    }

    [[nodiscard]] static bool cacheable(
        const RadialGradient& gradient) noexcept {
        const auto center = gradient.center();
        return std::isfinite(center.x) &&
               std::isfinite(center.y) &&
               std::isfinite(gradient.radius()) &&
               gradient.radius() > 0.0f &&
               gradient_cache_detail::cacheable_stops(gradient.stops());
    }

    [[nodiscard]] static sk_sp<SkShader> materialize(
        const LinearGradient& gradient) {
        const auto start = gradient.start();
        const auto end = gradient.end();
        const SkPoint points[2] = {
            {start.x, start.y},
            {end.x, end.y},
        };
        return gradient_cache_detail::materialize(
            gradient.stops(),
            [&points](const SkGradient& value) {
                return SkShaders::LinearGradient(points, value);
            });
    }

    [[nodiscard]] static sk_sp<SkShader> materialize(
        const RadialGradient& gradient) {
        const auto center = gradient.center();
        const SkPoint point{center.x, center.y};
        return gradient_cache_detail::materialize(
            gradient.stops(),
            [&gradient, point](const SkGradient& value) {
                if (!(gradient.radius() > 0.0f) ||
                    !std::isfinite(gradient.radius())) {
                    return sk_sp<SkShader>{};
                }
                return SkShaders::RadialGradient(
                    point, gradient.radius(), value);
            });
    }
};

} // namespace ui::detail
