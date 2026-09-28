#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>

namespace ui {

/// Axis used when projecting a two-dimensional pointer delta to one scalar.
enum class DragAxis {
    Horizontal,
    Vertical
};

/// Current state of `DragGesture`.
///
/// `Pressed` means a pointer is down but has not crossed the drag threshold.
/// `Dragging` begins on the first move whose total Euclidean distance reaches
/// the configured threshold.
enum class GesturePhase {
    Idle,
    Pressed,
    Dragging
};

/// Result of one drag-gesture transition.
///
/// `delta` is movement since the previous sample; `total` is movement from
/// the press origin. `drag_started` is true only on the threshold-crossing
/// update. `clicked` is reported only by `end()` when the threshold was never
/// crossed. `ended` and `cancelled` are mutually exclusive terminal markers.
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
    /// Construct with a non-negative logical-pixel drag threshold. Negative
    /// values are clamped to zero.
    explicit constexpr DragGesture(float threshold = 3.0f) noexcept
        : threshold_(std::max(0.0f, threshold)) {}

    /// Start/restart a gesture at `position` in the caller's logical
    /// coordinate space.
    void begin(Point position) noexcept {
        origin_ = position;
        current_ = position;
        phase_ = GesturePhase::Pressed;
    }

    /// Advance the active gesture to `position`.
    ///
    /// Returns an empty update while idle. The first sample whose total
    /// Euclidean distance reaches `threshold()` reports `drag_started=true`.
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

    /// Finish the gesture at `position`.
    ///
    /// Ending before the threshold is crossed reports a click; ending after a
    /// drag reports `dragging=true` and never `clicked`.
    [[nodiscard]] DragUpdate end(Point position) noexcept {
        if (!active()) return {};

        auto update = move(position);
        update.clicked = phase_ == GesturePhase::Pressed;
        update.dragging = phase_ == GesturePhase::Dragging;
        update.ended = true;
        phase_ = GesturePhase::Idle;
        return update;
    }

    /// Cancel the active gesture and return its final total displacement.
    /// Cancellation never reports a click.
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

/// Project a logical two-dimensional delta onto one configured drag axis.
[[nodiscard]] constexpr float drag_axis_delta(Point delta, DragAxis axis) noexcept {
    return axis == DragAxis::Horizontal ? delta.x : delta.y;
}

/// Convert a logical pointer delta into an application value delta.
///
/// `value_per_pixel` is multiplied by the selected axis displacement. Set
/// `invert` for controls whose increasing visual direction maps to decreasing
/// values. This helper does not clamp the resulting application value.
[[nodiscard]] constexpr float drag_value_delta(
    Point delta,
    DragAxis axis,
    float value_per_pixel,
    bool invert = false) noexcept {
    const float signed_delta = drag_axis_delta(delta, axis) * value_per_pixel;
    return invert ? -signed_delta : signed_delta;
}

} // namespace ui
