#pragma once

#include <algorithm>
#include <cmath>

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
};

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
