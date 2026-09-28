/// \file
/// Backend-neutral vector path and stroke descriptions used by Painter.
#pragma once

#include <nativeui/geometry.hpp>

#include <vector>

namespace ui {

class Painter;

/// Endpoint shape for open stroked segments.
enum class StrokeCap {
    Butt,
    Round,
    Square,
};

/// Join shape where stroked segments meet.
enum class StrokeJoin {
    Miter,
    Round,
    Bevel,
};

/// Stroke geometry.
///
/// Width is a logical UI length. miter_limit is a dimensionless stroke-width
/// ratio used when join == StrokeJoin::Miter.
struct StrokeStyle {
    float width{1.0f};
    StrokeCap cap{StrokeCap::Butt};
    StrokeJoin join{StrokeJoin::Miter};
    float miter_limit{4.0f};
};

/// Owned vector-command sequence in logical coordinates.
///
/// Coordinates are stored verbatim. Command appends may grow the owned vector
/// and allocate/throw. Painter::scoped_clip() rejects non-finite path geometry
/// by installing an empty clip; draw calls otherwise expect finite input.
class Path {
public:
    /// Start a new contour at point; returns *this for fluent construction.
    Path& move_to(Point point) {
        commands_.push_back(Command{Verb::Move, point, {}, {}});
        return *this;
    }

    /// Append a straight segment to point.
    Path& line_to(Point point) {
        commands_.push_back(Command{Verb::Line, point, {}, {}});
        return *this;
    }

    /// Append a quadratic Bézier segment.
    Path& quad_to(Point control, Point end) {
        commands_.push_back(Command{Verb::Quad, control, end, {}});
        return *this;
    }

    /// Append a cubic Bézier segment.
    Path& cubic_to(Point control1, Point control2, Point end) {
        commands_.push_back(Command{Verb::Cubic, control1, control2, end});
        return *this;
    }

    /// Close the current contour.
    Path& close() {
        commands_.push_back(Command{Verb::Close, {}, {}, {}});
        return *this;
    }

    /// Remove all commands while retaining vector capacity for reuse.
    void clear() noexcept { commands_.clear(); }
    /// Return true when no commands are stored.
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

private:
    enum class Verb {
        Move,
        Line,
        Quad,
        Cubic,
        Close,
    };

    struct Command {
        Verb verb{};
        Point a{};
        Point b{};
        Point c{};
    };

    friend class Painter;
    std::vector<Command> commands_;
};

} // namespace ui
