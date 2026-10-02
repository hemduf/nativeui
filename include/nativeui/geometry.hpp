#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace ui {

/// Single-precision π constant used by NativeUI angular helpers.
///
/// Angles in the public geometry/paint APIs are expressed in radians. This is a
/// compile-time value with no ownership, initialization-order, threading, or
/// allocation concerns.
constexpr float kPi = 3.14159265358979323846f;

// -----------------------------------------------------------------------------
// Geometry / colors / input
// -----------------------------------------------------------------------------

/// Width/height pair expressed in NativeUI logical pixels unless an API states
/// another coordinate space explicitly.
struct Size {
    /// Horizontal extent in logical pixels unless the receiving API says otherwise.
    ///
    /// The value type performs no normalization: negative, infinite, and NaN
    /// payloads remain representable. Layout callers that require a valid extent
    /// should pass the value through `Constraints`.
    float w{};
    /// Vertical extent with the same units and validation contract as `w`.
    float h{};
};

/// Two-dimensional point expressed in NativeUI logical coordinates unless an
/// API states another coordinate space explicitly.
struct Point {
    /// Horizontal logical coordinate in the coordinate space named by the caller.
    ///
    /// No finite-value validation is performed by Point itself.
    float x{};
    /// Vertical logical coordinate in the coordinate space named by the caller.
    float y{};
};

/// Axis-aligned rectangle in logical coordinates.
///
/// `x`/`y` identify the top-left origin and `w`/`h` are extents. A rectangle
/// is empty unless both extents are strictly positive.
struct Rect {
    /// Logical X coordinate of the top-left origin.
    float x{};
    /// Logical Y coordinate of the top-left origin.
    float y{};
    /// Logical horizontal extent; non-positive or NaN makes `empty()` true.
    float w{};
    /// Logical vertical extent; non-positive or NaN makes `empty()` true.
    float h{};

    /// Returns true when either extent is zero, negative, NaN, or otherwise not
    /// strictly positive.
    [[nodiscard]] bool empty() const noexcept { return !(w > 0.0f && h > 0.0f); }

    /// Return whether `p` lies inside or on the rectangle edges.
    ///
    /// The point is copied by value and no storage is retained. This helper does
    /// not canonicalize the rectangle or point; non-finite coordinates therefore
    /// follow ordinary IEEE comparison rules and generally fail containment.
    [[nodiscard]] bool contains(Point p) const noexcept {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }

    /// Return whether the complete non-empty `other` rectangle lies inside.
    ///
    /// Coincident edges are included. Empty candidates are never contained. Both
    /// rectangles are copied value snapshots; no normalization, allocation, or
    /// callback occurs, so callers needing portable results should provide finite
    /// origins and extents.
    [[nodiscard]] bool contains(Rect other) const noexcept {
        return !other.empty() && other.x >= x && other.y >= y &&
               other.x + other.w <= x + w && other.y + other.h <= y + h;
    }
};

/// Returns the positive-area intersection of two rectangles, or `Rect{}` when
/// they do not overlap with positive area. Merely touching edges has no area.
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

/// Returns the smallest axis-aligned rectangle covering both inputs. An empty
/// input is ignored; if both are empty the result is empty.
[[nodiscard]] inline Rect unite(Rect a, Rect b) noexcept {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const float left = (std::min)(a.x, b.x);
    const float top = (std::min)(a.y, b.y);
    const float right = (std::max)(a.x + a.w, b.x + b.w);
    const float bottom = (std::max)(a.y + a.h, b.y + b.h);
    return Rect{left, top, right - left, bottom - top};
}

/// Returns true when two non-empty rectangles overlap or touch at an edge or
/// corner. Empty rectangles never overlap or touch.
[[nodiscard]] inline bool overlaps_or_touches(Rect a, Rect b) noexcept {
    if (a.empty() || b.empty()) return false;
    return a.x <= b.x + b.w && b.x <= a.x + a.w &&
           a.y <= b.y + b.h && b.y <= a.y + a.h;
}


/// Affine 2D transform stored as the upper two rows of a 3x3 matrix:
/// `[m00 m01 m02; m10 m11 m12; 0 0 1]`.
///
/// Points are treated as column vectors. For `a * b`, `b` is applied first
/// and `a` second.
struct Transform2D {
    /// First-row X coefficient (dimensionless scale/rotation/shear term).
    float m00{1.0f};
    /// First-row Y coefficient (dimensionless scale/rotation/shear term).
    float m01{};
    /// First-row translation term, in logical units.
    float m02{};
    /// Second-row X coefficient (dimensionless scale/rotation/shear term).
    float m10{};
    /// Second-row Y coefficient (dimensionless scale/rotation/shear term).
    float m11{1.0f};
    /// Second-row translation term, in logical units.
    float m12{};

    /// Returns the identity transform.
    [[nodiscard]] static constexpr Transform2D identity() noexcept { return {}; }
    /// Return a translation by `x` and `y` logical units.
    ///
    /// Inputs are stored verbatim; unlike `rotation()`, this constructor does not
    /// reject non-finite values. It is constexpr, allocation-free, and callback-free.
    [[nodiscard]] static constexpr Transform2D translation(float x, float y) noexcept {
        return Transform2D{1.0f, 0.0f, x, 0.0f, 1.0f, y};
    }
    /// Return independent dimensionless X/Y scaling around the origin.
    ///
    /// Inputs are stored verbatim, including zero, negative, or non-finite
    /// factors. A zero scale is valid to construct but is not invertible.
    [[nodiscard]] static constexpr Transform2D scaling(float x, float y) noexcept {
        return Transform2D{x, 0.0f, 0.0f, 0.0f, y, 0.0f};
    }

    /// Returns a rotation around the origin. Non-finite input falls back to the
    /// identity transform rather than propagating invalid matrix values.
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

    /// Map one copied Point through this affine transform.
    ///
    /// The result is an owned value. No validation or fallback is applied; a
    /// non-finite matrix/input may therefore produce non-finite output. The
    /// operation allocates nothing and invokes no callbacks.
    [[nodiscard]] Point map_point(Point point) const noexcept {
        return Point{
            m00 * point.x + m01 * point.y + m02,
            m10 * point.x + m11 * point.y + m12};
    }

    /// Returns the inverse transform when it is finite, numerically invertible,
    /// and representable as `float`; otherwise returns `std::nullopt`.
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
            static_cast<double>((std::numeric_limits<float>::max)());
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

/// Compose two borrowed transforms; the result applies `b` first and then `a`.
///
/// The returned matrix is an owned value. Multiplication performs no finite-value
/// repair or singularity check, allocates nothing, and invokes no callbacks.
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

/// RGBA color value. Alpha defaults to fully opaque; consuming APIs define
/// any additional clamping or color-space behavior.
struct Color {
    /// Red channel. The value is stored verbatim and is not clamped to [0, 1].
    float r{};
    /// Green channel with the same unclamped scalar contract as `r`.
    float g{};
    /// Blue channel with the same unclamped scalar contract as `r`.
    float b{};
    /// Alpha channel; defaults to fully opaque 1.0 and is not clamped.
    float a{1.0f};
};

/// Built-in convenience palette used by NativeUI's default visual language.
namespace colors {
/// Default application/background surface.
constexpr Color background{0.055f, 0.060f, 0.070f, 1.0f};
/// Default elevated panel surface.
constexpr Color panel{0.090f, 0.098f, 0.112f, 1.0f};
/// Default neutral control/panel border.
constexpr Color border{0.235f, 0.250f, 0.275f, 1.0f};
/// Default focused-border highlight.
constexpr Color borderFocus{0.95f, 0.62f, 0.24f, 1.0f};
/// Default primary foreground text.
constexpr Color text{0.92f, 0.93f, 0.95f, 1.0f};
/// Default secondary/muted foreground text.
constexpr Color textMuted{0.56f, 0.59f, 0.64f, 1.0f};
/// Default accent/highlight color.
constexpr Color accent{0.95f, 0.62f, 0.24f, 1.0f};
/// Default knob outer surface.
constexpr Color knob{0.20f, 0.22f, 0.25f, 1.0f};
/// Default knob inner surface.
constexpr Color knobInner{0.10f, 0.11f, 0.125f, 1.0f};
/// Default inactive track surface.
constexpr Color track{0.28f, 0.30f, 0.34f, 1.0f};
/// Default off-state toggle surface.
constexpr Color toggleOff{0.20f, 0.22f, 0.25f, 1.0f};
/// Default text-input surface.
constexpr Color input{0.070f, 0.076f, 0.088f, 1.0f};
/// Default translucent selection highlight.
constexpr Color selection{0.95f, 0.62f, 0.24f, 0.30f};
/// Default text caret color.
constexpr Color caret{0.98f, 0.82f, 0.58f, 1.0f};
} // namespace colors


} // namespace ui
