#pragma once

#include <nativeui/geometry.hpp>

#include <vector>

namespace ui {

class Painter;

enum class StrokeCap {
    Butt,
    Round,
    Square,
};

enum class StrokeJoin {
    Miter,
    Round,
    Bevel,
};

struct StrokeStyle {
    float width{1.0f};
    StrokeCap cap{StrokeCap::Butt};
    StrokeJoin join{StrokeJoin::Miter};
    float miter_limit{4.0f};
};

class Path {
public:
    Path& move_to(Point point) {
        commands_.push_back(Command{Verb::Move, point, {}, {}});
        return *this;
    }

    Path& line_to(Point point) {
        commands_.push_back(Command{Verb::Line, point, {}, {}});
        return *this;
    }

    Path& quad_to(Point control, Point end) {
        commands_.push_back(Command{Verb::Quad, control, end, {}});
        return *this;
    }

    Path& cubic_to(Point control1, Point control2, Point end) {
        commands_.push_back(Command{Verb::Cubic, control1, control2, end});
        return *this;
    }

    Path& close() {
        commands_.push_back(Command{Verb::Close, {}, {}, {}});
        return *this;
    }

    void clear() noexcept { commands_.clear(); }
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
