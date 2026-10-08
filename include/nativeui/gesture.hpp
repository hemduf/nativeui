#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>

namespace ui {

/// Axis used when projecting a two-dimensional pointer delta to one scalar.
///
/// The enum only selects a Point component; it does not transform coordinate
/// spaces or retain any input state.
enum class DragAxis {
    /// Select the x component.
    Horizontal,
    /// Select the y component.
    Vertical
};

/// Current state of `DragGesture`.
///
/// `Pressed` means a pointer is down but has not crossed the drag threshold.
/// `Dragging` begins on the first move whose total Euclidean distance reaches
/// the configured threshold.
enum class GesturePhase {
    /// No gesture is active.
    Idle,
    /// A gesture is active but has not crossed the drag threshold.
    Pressed,
    /// The active gesture has reached or crossed the drag threshold.
    Dragging
};

/// Result of one drag-gesture transition.
///
/// `delta` is movement since the previous sample; `total` is movement from
/// the press origin. `drag_started` is true only on the threshold-crossing
/// update. `clicked` is reported only by `end()` when the threshold was never
/// crossed. `ended` and `cancelled` are mutually exclusive terminal markers.
struct DragUpdate {
    /// Movement from the previous sampled position to this sample.
    ///
    /// Uses the same coordinate units as begin()/move()/end(); cancel() does
    /// not sample a new position and therefore returns a zero delta.
    Point delta{};

    /// Movement from the most recent begin() origin to this sample.
    Point total{};

    /// True only on the first sample that reaches/crosses the threshold.
    bool drag_started{};

    /// Whether this returned snapshot represents the Dragging phase.
    ///
    /// end() can return true here even though the gesture has already become
    /// Idle internally before end() returns.
    bool dragging{};

    /// True only for end() of a gesture that never reached Dragging.
    bool clicked{};

    /// True only for a successful end() terminal snapshot.
    bool ended{};

    /// True only for a successful cancel() terminal snapshot.
    bool cancelled{};
};

/// Small synchronous click/drag state machine.
///
/// The helper owns only scalar/Point state and performs no allocation, callback
/// dispatch, retained-tree access, pointer capture, or platform I/O. Copies and
/// moves are independent value snapshots. One instance has no synchronization
/// and should be mutated by one thread at a time; NativeUI controls normally
/// drive it from their owning UI thread.
///
/// A press becomes a drag once a sampled position reaches the Euclidean distance
/// threshold from the current origin. Releasing before that threshold reports a
/// click. Pointer capture is deliberately the caller's InputContext policy.
class DragGesture {
public:
    /// Construct with a drag threshold in the caller's coordinate units.
    ///
    /// NativeUI pointer positions normally use logical pixels. Negative values
    /// are clamped to zero. No coordinate conversion, callbacks, or allocation
    /// occur during construction.
    explicit constexpr DragGesture(float threshold = 3.0f) noexcept
        : threshold_(std::max(0.0f, threshold)) {}

    /// Start or restart a gesture at `position`.
    ///
    /// The position is copied as both origin and current sample. Restarting an
    /// already-active gesture abandons the previous one without synthesizing an
    /// end/cancel notification.
    void begin(Point position) noexcept {
        origin_ = position;
        current_ = position;
        phase_ = GesturePhase::Pressed;
    }

    /// Advance the active gesture to `position`.
    ///
    /// Idle calls return a zero/default update and do not mutate state. Active
    /// calls report delta from the previous sample and total from the begin()
    /// origin. The first sample whose Euclidean total reaches threshold()
    /// transitions to Dragging and reports drag_started=true.
    ///
    /// A zero threshold starts dragging on the first move sample, even with zero
    /// displacement. Non-finite Point values are not normalized.
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
    /// The final position is sampled with the same threshold logic as move()
    /// before click/drag classification. Idle calls return a zero/default update.
    /// Ending while still Pressed reports clicked; ending after Dragging reports
    /// dragging=true and never clicked. The internal phase becomes Idle before
    /// return. With a zero threshold, even an unchanged final position crosses
    /// the threshold and therefore does not report a click.
    [[nodiscard]] DragUpdate end(Point position) noexcept {
        if (!active()) return {};

        auto update = move(position);
        update.clicked = phase_ == GesturePhase::Pressed;
        update.dragging = phase_ == GesturePhase::Dragging;
        update.ended = true;
        phase_ = GesturePhase::Idle;
        return update;
    }

    /// Cancel the active gesture without sampling a new position.
    ///
    /// Idle calls return a zero/default update. Active cancellation reports total
    /// displacement at the last current() sample, preserves whether Dragging had
    /// started, sets cancelled=true, leaves delta zero, and returns to Idle.
    /// Cancellation never reports clicked or ended.
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

    /// Return whether the phase is Pressed or Dragging.
    [[nodiscard]] constexpr bool active() const noexcept {
        return phase_ != GesturePhase::Idle;
    }

    /// Return whether the current phase is Dragging.
    [[nodiscard]] constexpr bool dragging() const noexcept {
        return phase_ == GesturePhase::Dragging;
    }

    /// Return the current phase without mutating state.
    [[nodiscard]] constexpr GesturePhase phase() const noexcept { return phase_; }

    /// Return the last begin() origin in caller coordinate units.
    ///
    /// The value remains available after termination; phase()/active() indicate
    /// whether it belongs to a currently active gesture.
    [[nodiscard]] constexpr Point origin() const noexcept { return origin_; }

    /// Return the most recently sampled position in caller coordinate units.
    ///
    /// end()/cancel() leave the stored value intact after transitioning to Idle.
    [[nodiscard]] constexpr Point current() const noexcept { return current_; }

    /// Return the constructor threshold after non-negative clamping.
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

/// Project a two-dimensional delta onto one configured drag axis.
///
/// The result has the same units as the selected Point component (normally
/// logical pixels). No allocation, callback, clamping, or coordinate conversion
/// occurs.
[[nodiscard]] constexpr float drag_axis_delta(Point delta, DragAxis axis) noexcept {
    return axis == DragAxis::Horizontal ? delta.x : delta.y;
}

/// Convert a logical pointer delta into an application value delta.
///
/// `value_per_pixel` is application-value units per selected Point unit
/// (normally per logical pixel). It is multiplied directly by the selected
/// displacement; `invert` negates the product for controls whose increasing
/// visual direction maps to decreasing values.
///
/// The helper does not clamp the application domain, validate finite inputs,
/// retain state, or invoke callbacks.
[[nodiscard]] constexpr float drag_value_delta(
    Point delta,
    DragAxis axis,
    float value_per_pixel,
    bool invert = false) noexcept {
    const float signed_delta = drag_axis_delta(delta, axis) * value_per_pixel;
    return invert ? -signed_delta : signed_delta;
}

} // namespace ui
