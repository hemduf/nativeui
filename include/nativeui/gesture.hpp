#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>

namespace ui {

enum class DragAxis {
    Horizontal,
    Vertical
};

enum class GesturePhase {
    Idle,
    Pressed,
    Dragging
};

struct DragUpdate {
    Point delta{};
    Point total{};
    bool drag_started{};
    bool dragging{};
    bool clicked{};
    bool ended{};
    bool cancelled{};
};

/// Small synchronous click/drag state machine.
///
/// The helper stores only value types and performs no heap allocation. A press
/// becomes a drag once the Euclidean distance from the press origin reaches the
/// configured threshold. Releasing before that threshold reports a click.
class DragGesture {
public:
    explicit constexpr DragGesture(float threshold = 3.0f) noexcept
        : threshold_(std::max(0.0f, threshold)) {}

    void begin(Point position) noexcept {
        origin_ = position;
        current_ = position;
        phase_ = GesturePhase::Pressed;
    }

    [[nodiscard]] DragUpdate move(Point position) noexcept {
        if (!active()) return {};

        const Point step{position.x - current_.x, position.y - current_.y};
        const Point total{position.x - origin_.x, position.y - origin_.y};
        current_ = position;

        bool started = false;
        if (phase_ == GesturePhase::Pressed && distance_squared(total) >= threshold_ * threshold_) {
            phase_ = GesturePhase::Dragging;
            started = true;
        }

        return DragUpdate{
            .delta = step,
            .total = total,
            .drag_started = started,
            .dragging = phase_ == GesturePhase::Dragging};
    }

    [[nodiscard]] DragUpdate end(Point position) noexcept {
        if (!active()) return {};

        auto update = move(position);
        update.clicked = phase_ == GesturePhase::Pressed;
        update.dragging = phase_ == GesturePhase::Dragging;
        update.ended = true;
        phase_ = GesturePhase::Idle;
        return update;
    }

    [[nodiscard]] DragUpdate cancel() noexcept {
        if (!active()) return {};

        const Point total{current_.x - origin_.x, current_.y - origin_.y};
        const bool was_dragging = phase_ == GesturePhase::Dragging;
        phase_ = GesturePhase::Idle;
        return DragUpdate{
            .total = total,
            .dragging = was_dragging,
            .cancelled = true};
    }

    [[nodiscard]] constexpr bool active() const noexcept {
        return phase_ != GesturePhase::Idle;
    }

    [[nodiscard]] constexpr bool dragging() const noexcept {
        return phase_ == GesturePhase::Dragging;
    }

    [[nodiscard]] constexpr GesturePhase phase() const noexcept { return phase_; }
    [[nodiscard]] constexpr Point origin() const noexcept { return origin_; }
    [[nodiscard]] constexpr Point current() const noexcept { return current_; }
    [[nodiscard]] constexpr float threshold() const noexcept { return threshold_; }

private:
    [[nodiscard]] static constexpr float distance_squared(Point point) noexcept {
        return point.x * point.x + point.y * point.y;
    }

    float threshold_{3.0f};
    Point origin_{};
    Point current_{};
    GesturePhase phase_{GesturePhase::Idle};
};

[[nodiscard]] constexpr float drag_axis_delta(Point delta, DragAxis axis) noexcept {
    return axis == DragAxis::Horizontal ? delta.x : delta.y;
}

[[nodiscard]] constexpr float drag_value_delta(
    Point delta,
    DragAxis axis,
    float value_per_pixel,
    bool invert = false) noexcept {
    const float signed_delta = drag_axis_delta(delta, axis) * value_per_pixel;
    return invert ? -signed_delta : signed_delta;
}

} // namespace ui
