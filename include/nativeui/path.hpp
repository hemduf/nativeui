/// \file
/// Backend-neutral vector path and stroke descriptions used by Painter.
#pragma once

#include <nativeui/geometry.hpp>

#include <vector>

namespace ui {

class Painter;

/// Endpoint shape for open stroked segments.
enum class StrokeCap {
    /// End exactly at the segment endpoint.
    Butt,
    /// Add a semicircular cap centered on the endpoint.
    Round,
    /// Extend by half the stroke width with a square edge.
    Square,
};

/// Join shape where stroked segments meet.
enum class StrokeJoin {
    /// Extend outer edges to their intersection, subject to miter_limit.
    Miter,
    /// Join segments with a circular arc.
    Round,
    /// Truncate the outer corner.
    Bevel,
};

/// Stroke geometry.
///
/// Width is a logical UI length. miter_limit is a dimensionless stroke-width
/// ratio used when join == StrokeJoin::Miter.
struct StrokeStyle {
    /// Stroke width in logical UI units. Painter treats width <= 0 as a no-op;
    /// use a finite positive value for deterministic rendering.
    float width{1.0f};
    /// Endpoint treatment for open contours.
    StrokeCap cap{StrokeCap::Butt};
    /// Corner treatment between connected segments.
    StrokeJoin join{StrokeJoin::Miter};
    /// Dimensionless miter-length / stroke-width ratio. Negative values clamp
    /// to zero before backend use.
    float miter_limit{4.0f};
};

/// Owned vector-command sequence in logical coordinates.
///
/// Path retains no Painter/backend resource and no references to caller points.
/// Coordinates are stored verbatim. Command appends may grow the owned vector
/// and allocate/throw; copying a Path copies the command vector and may allocate.
/// clear() is allocation-free and keeps capacity for reuse.
///
/// Path has no internal synchronization. Build/mutate it in an owned thread or
/// externally synchronize access. Allocation/materialization make it unsuitable
/// for direct audio/DSP real-time callbacks.
///
/// Painter::scoped_clip() rejects non-finite path geometry by installing an
/// empty clip; fill/stroke draw calls otherwise expect finite input.
class Path {
public:
    /// Start a new contour at point in logical coordinates.
    ///
    /// The point is copied verbatim. Returns *this; vector growth may allocate.
    Path& move_to(Point point) {
        commands_.push_back(Command{Verb::Move, point, {}, {}});
        return *this;
    }

    /// Append a straight segment to point in logical coordinates.
    ///
    /// No finiteness validation occurs here. Returns *this; vector growth may allocate.
    Path& line_to(Point point) {
        commands_.push_back(Command{Verb::Line, point, {}, {}});
        return *this;
    }

    /// Append a quadratic Bézier segment in logical coordinates.
    ///
    /// control/end are copied verbatim. Returns *this; vector growth may allocate.
    Path& quad_to(Point control, Point end) {
        commands_.push_back(Command{Verb::Quad, control, end, {}});
        return *this;
    }

    /// Append a cubic Bézier segment in logical coordinates.
    ///
    /// Both controls and end are copied verbatim. Returns *this; growth may allocate.
    Path& cubic_to(Point control1, Point control2, Point end) {
        commands_.push_back(Command{Verb::Cubic, control1, control2, end});
        return *this;
    }

    /// Close the current contour by appending a close command.
    ///
    /// Geometry is not synthesized eagerly; Painter interprets it synchronously.
    /// Returns *this; vector growth may allocate.
    Path& close() {
        commands_.push_back(Command{Verb::Close, {}, {}, {}});
        return *this;
    }

    /// Remove all commands while retaining vector capacity for reuse.
    ///
    /// Painter borrows a Path only for a synchronous call, so reuse is safe after
    /// that call returns. The operation itself does not allocate.
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
