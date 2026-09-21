#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace ui {

constexpr float kPi = 3.14159265358979323846f;

// -----------------------------------------------------------------------------
// Geometry / colors / input
// -----------------------------------------------------------------------------

struct Size {
    float w{};
    float h{};
};

struct Point {
    float x{};
    float y{};
};

struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};

    [[nodiscard]] bool empty() const noexcept { return !(w > 0.0f && h > 0.0f); }

    [[nodiscard]] bool contains(Point p) const noexcept {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }

    [[nodiscard]] bool contains(Rect other) const noexcept {
        return !other.empty() && other.x >= x && other.y >= y &&
               other.x + other.w <= x + w && other.y + other.h <= y + h;
    }
};

[[nodiscard]] inline Rect intersect(Rect a, Rect b) noexcept {
    // Keep public geometry usable even when a consumer included windows.h
    // without NOMINMAX before NativeUI. Parenthesized std::min/std::max names
    // cannot be captured by the Win32 function-like macros.
    const float left = (std::max)(a.x, b.x);
    const float top = (std::max)(a.y, b.y);
    const float right = (std::min)(a.x + a.w, b.x + b.w);
    const float bottom = (std::min)(a.y + a.h, b.y + b.h);
    return right > left && bottom > top
        ? Rect{left, top, right - left, bottom - top}
        : Rect{};
}

[[nodiscard]] inline Rect unite(Rect a, Rect b) noexcept {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const float left = (std::min)(a.x, b.x);
    const float top = (std::min)(a.y, b.y);
    const float right = (std::max)(a.x + a.w, b.x + b.w);
    const float bottom = (std::max)(a.y + a.h, b.y + b.h);
    return Rect{left, top, right - left, bottom - top};
}

[[nodiscard]] inline bool overlaps_or_touches(Rect a, Rect b) noexcept {
    if (a.empty() || b.empty()) return false;
    return a.x <= b.x + b.w && b.x <= a.x + a.w &&
           a.y <= b.y + b.h && b.y <= a.y + a.h;
}


struct Transform2D {
    float m00{1.0f};
    float m01{};
    float m02{};
    float m10{};
    float m11{1.0f};
    float m12{};

    [[nodiscard]] static constexpr Transform2D identity() noexcept { return {}; }
    [[nodiscard]] static constexpr Transform2D translation(float x, float y) noexcept {
        return Transform2D{1.0f, 0.0f, x, 0.0f, 1.0f, y};
    }
    [[nodiscard]] static constexpr Transform2D scaling(float x, float y) noexcept {
        return Transform2D{x, 0.0f, 0.0f, 0.0f, y, 0.0f};
    }

    [[nodiscard]] static Transform2D rotation(float radians) noexcept {
        if (!std::isfinite(radians)) return identity();

        const double angle = static_cast<double>(radians);
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        if (!std::isfinite(cosine) || !std::isfinite(sine)) return identity();

        return Transform2D{
            static_cast<float>(cosine), static_cast<float>(-sine), 0.0f,
            static_cast<float>(sine), static_cast<float>(cosine), 0.0f};
    }

    [[nodiscard]] Point map_point(Point point) const noexcept {
        return Point{
            m00 * point.x + m01 * point.y + m02,
            m10 * point.x + m11 * point.y + m12};
    }

    [[nodiscard]] std::optional<Transform2D> inverse() const noexcept {
        const double a = static_cast<double>(m00);
        const double c = static_cast<double>(m01);
        const double tx = static_cast<double>(m02);
        const double b = static_cast<double>(m10);
        const double d = static_cast<double>(m11);
        const double ty = static_cast<double>(m12);

        if (!std::isfinite(a) || !std::isfinite(c) || !std::isfinite(tx) ||
            !std::isfinite(b) || !std::isfinite(d) || !std::isfinite(ty)) {
            return std::nullopt;
        }

        const double scale = (std::max)(
            (std::max)(std::abs(a), std::abs(b)),
            (std::max)(std::abs(c), std::abs(d)));
        if (scale == 0.0) return std::nullopt;

        const double an = a / scale;
        const double bn = b / scale;
        const double cn = c / scale;
        const double dn = d / scale;
        const double normalized_determinant = an * dn - bn * cn;
        if (!std::isfinite(normalized_determinant) ||
            std::abs(normalized_determinant) <= 1.0e-8) {
            return std::nullopt;
        }

        const double determinant = a * d - b * c;
        if (!std::isfinite(determinant) || determinant == 0.0) {
            return std::nullopt;
        }

        const double inverse_values[6] = {
            d / determinant,
            -c / determinant,
            (c * ty - d * tx) / determinant,
            -b / determinant,
            a / determinant,
            (b * tx - a * ty) / determinant,
        };

        constexpr double max_float =
            static_cast<double>(std::numeric_limits<float>::max());
        for (const double value : inverse_values) {
            if (!std::isfinite(value) || std::abs(value) > max_float) {
                return std::nullopt;
            }
        }

        return Transform2D{
            static_cast<float>(inverse_values[0]),
            static_cast<float>(inverse_values[1]),
            static_cast<float>(inverse_values[2]),
            static_cast<float>(inverse_values[3]),
            static_cast<float>(inverse_values[4]),
            static_cast<float>(inverse_values[5]),
        };
    }
};

[[nodiscard]] inline Transform2D operator*(const Transform2D& a,
                                           const Transform2D& b) noexcept {
    return Transform2D{
        a.m00 * b.m00 + a.m01 * b.m10,
        a.m00 * b.m01 + a.m01 * b.m11,
        a.m00 * b.m02 + a.m01 * b.m12 + a.m02,
        a.m10 * b.m00 + a.m11 * b.m10,
        a.m10 * b.m01 + a.m11 * b.m11,
        a.m10 * b.m02 + a.m11 * b.m12 + a.m12,
    };
}

struct Color {
    float r{};
    float g{};
    float b{};
    float a{1.0f};
};

namespace colors {
constexpr Color background{0.055f, 0.060f, 0.070f, 1.0f};
constexpr Color panel{0.090f, 0.098f, 0.112f, 1.0f};
constexpr Color border{0.235f, 0.250f, 0.275f, 1.0f};
constexpr Color borderFocus{0.95f, 0.62f, 0.24f, 1.0f};
constexpr Color text{0.92f, 0.93f, 0.95f, 1.0f};
constexpr Color textMuted{0.56f, 0.59f, 0.64f, 1.0f};
constexpr Color accent{0.95f, 0.62f, 0.24f, 1.0f};
constexpr Color knob{0.20f, 0.22f, 0.25f, 1.0f};
constexpr Color knobInner{0.10f, 0.11f, 0.125f, 1.0f};
constexpr Color track{0.28f, 0.30f, 0.34f, 1.0f};
constexpr Color toggleOff{0.20f, 0.22f, 0.25f, 1.0f};
constexpr Color input{0.070f, 0.076f, 0.088f, 1.0f};
constexpr Color selection{0.95f, 0.62f, 0.24f, 0.30f};
constexpr Color caret{0.98f, 0.82f, 0.58f, 1.0f};
} // namespace colors


} // namespace ui
