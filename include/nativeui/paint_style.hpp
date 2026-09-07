#pragma once

#include <nativeui/geometry.hpp>

#include <initializer_list>
#include <vector>

namespace ui {

struct GradientStop {
    float offset{};
    Color color{};
};

class LinearGradient {
public:
    LinearGradient(Point start, Point end, Color start_color, Color end_color)
        : start_(start), end_(end), stops_{{0.0f, start_color}, {1.0f, end_color}} {}

    LinearGradient(Point start, Point end, std::initializer_list<GradientStop> stops)
        : start_(start), end_(end), stops_(stops) {}

    [[nodiscard]] Point start() const noexcept { return start_; }
    [[nodiscard]] Point end() const noexcept { return end_; }
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

    // Preserve the original two-stop convenience surface while generalized
    // gradients expose their complete immutable stop list through stops().
    [[nodiscard]] Color start_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.front().color;
    }
    [[nodiscard]] Color end_color() const noexcept {
        return stops_.empty() ? Color{} : stops_.back().color;
    }

private:
    Point start_{};
    Point end_{};
    std::vector<GradientStop> stops_;
};

class RadialGradient {
public:
    RadialGradient(Point center, float radius, Color inner_color, Color outer_color)
        : center_(center), radius_(radius), stops_{{0.0f, inner_color}, {1.0f, outer_color}} {}

    RadialGradient(Point center, float radius, std::initializer_list<GradientStop> stops)
        : center_(center), radius_(radius), stops_(stops) {}

    [[nodiscard]] Point center() const noexcept { return center_; }
    [[nodiscard]] float radius() const noexcept { return radius_; }
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

private:
    Point center_{};
    float radius_{};
    std::vector<GradientStop> stops_;
};

} // namespace ui
