#pragma once

#include <nativeui/noise.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace ui {

/// Channel to extract from a Brush-backed scalar source (red, green, blue or alpha).
enum class ScalarChannel {
    Red,
    Green,
    Blue,
    Alpha,
};

namespace detail {
struct ScalarSourceAccess;
}

/// Owned scalar value for numeric/material inputs, independent of Pugl/Skia public types.
/// It stores either an exact float or an owned Brush plus channel.
/// Construction from Brush and copying may allocate; do not use in real-time audio callbacks.
class ScalarSource final {
public:
/// Default source is the constant zero and requires no renderer resource.
    ScalarSource() noexcept : value_(0.0f) {}
/// Store value verbatim; NaN and infinity are not normalized by the scalar wrapper.
    explicit ScalarSource(float value) noexcept : value_(value) {}

/// Construct an unclamped constant without allocating.
    [[nodiscard]] static ScalarSource constant(float value) noexcept {
        return ScalarSource{value};
    }

/// Own the supplied Brush and sample the selected channel when consumed.
/// An out-of-range ScalarChannel returns the default zero source.
    [[nodiscard]] static ScalarSource from_brush(
        Brush brush,
        ScalarChannel channel) {
        if (!valid_channel(channel)) return ScalarSource{};
        return ScalarSource{BrushChannel{std::move(brush), channel}};
    }

/// Adapt a NoiseSource through its opaque black-to-white Brush and red channel.
/// Inspect NoiseCreateResult before passing a procedural source; no new compilation occurs here.
    [[nodiscard]] static ScalarSource from_noise(NoiseSource noise) {
        return from_brush(
            noise.as_brush({0.0f, 0.0f, 0.0f, 1.0f},
                           {1.0f, 1.0f, 1.0f, 1.0f}),
            ScalarChannel::Red);
    }

/// Copying retains independent value semantics; a Brush-backed copy can allocate.
    ScalarSource(const ScalarSource&) = default;

/// Copy-assign via a prepared replacement; a failed copy leaves this source unchanged.
    ScalarSource& operator=(const ScalarSource& other) {
        if (this == &other) return *this;
        ScalarSource replacement{other};
        *this = std::move(replacement);
        return *this;
    }

/// Move without throwing; the moved-from source becomes constant zero.
    ScalarSource(ScalarSource&& other) noexcept : value_(0.0f) {
        value_ = std::move(other.value_);
        other.reset_to_zero();
    }

/// Move-assign without throwing. Self-move intentionally resets this source to zero.
    ScalarSource& operator=(ScalarSource&& other) noexcept {
        if (this == &other) {
            reset_to_zero();
            return *this;
        }
        value_ = std::move(other.value_);
        other.reset_to_zero();
        return *this;
    }

    ~ScalarSource() noexcept = default;

private:
    struct BrushChannel {
        Brush brush;
        ScalarChannel channel{ScalarChannel::Red};
    };

    using Storage = std::variant<float, BrushChannel>;

    explicit ScalarSource(BrushChannel source)
        : value_(std::move(source)) {}

    [[nodiscard]] static constexpr bool valid_channel(
        ScalarChannel channel) noexcept {
        switch (channel) {
        case ScalarChannel::Red:
        case ScalarChannel::Green:
        case ScalarChannel::Blue:
        case ScalarChannel::Alpha:
            return true;
        }
        return false;
    }

    void reset_to_zero() noexcept {
        Storage replacement{0.0f};
        value_ = std::move(replacement);
    }

    friend struct detail::ScalarSourceAccess;
    Storage value_;
};

static_assert(std::is_nothrow_default_constructible_v<ScalarSource>);
static_assert(std::is_nothrow_constructible_v<ScalarSource, float>);
static_assert(std::is_copy_constructible_v<ScalarSource>);
static_assert(std::is_nothrow_move_constructible_v<ScalarSource>);
static_assert(std::is_nothrow_move_assignable_v<ScalarSource>);
static_assert(std::is_nothrow_destructible_v<ScalarSource>);

} // namespace ui
