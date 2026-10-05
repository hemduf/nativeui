#pragma once

#include <nativeui/noise.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace ui {

enum class ScalarChannel {
    Red,
    Green,
    Blue,
    Alpha,
};

namespace detail {
struct ScalarSourceAccess;
}

class ScalarSource final {
public:
    ScalarSource() noexcept : value_(0.0f) {}
    explicit ScalarSource(float value) noexcept : value_(value) {}

    [[nodiscard]] static ScalarSource constant(float value) noexcept {
        return ScalarSource{value};
    }

    [[nodiscard]] static ScalarSource from_brush(
        Brush brush,
        ScalarChannel channel) {
        if (!valid_channel(channel)) return ScalarSource{};
        return ScalarSource{BrushChannel{std::move(brush), channel}};
    }

    [[nodiscard]] static ScalarSource from_noise(NoiseSource noise) {
        return from_brush(
            noise.as_brush({0.0f, 0.0f, 0.0f, 1.0f},
                           {1.0f, 1.0f, 1.0f, 1.0f}),
            ScalarChannel::Red);
    }

    ScalarSource(const ScalarSource&) = default;

    ScalarSource& operator=(const ScalarSource& other) {
        if (this == &other) return *this;
        ScalarSource replacement{other};
        *this = std::move(replacement);
        return *this;
    }

    ScalarSource(ScalarSource&& other) noexcept : value_(0.0f) {
        value_ = std::move(other.value_);
        other.reset_to_zero();
    }

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
