#pragma once

#include <nativeui/geometry.hpp>

namespace ui {

class LinearGradient {
public:
    constexpr LinearGradient(Point start, Point end, Color start_color, Color end_color) noexcept
        : start_(start), end_(end), start_color_(start_color), end_color_(end_color) {}

    [[nodiscard]] constexpr Point start() const noexcept { return start_; }
    [[nodiscard]] constexpr Point end() const noexcept { return end_; }
    [[nodiscard]] constexpr Color start_color() const noexcept { return start_color_; }
    [[nodiscard]] constexpr Color end_color() const noexcept { return end_color_; }

private:
    Point start_{};
    Point end_{};
    Color start_color_{};
    Color end_color_{};
};

} // namespace ui
