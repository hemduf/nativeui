#include <nativeui/paint.hpp>

static_assert(requires(ui::Painter& painter,
                       ui::Rect rect,
                       ui::Point a,
                       ui::Point b,
                       const ui::Path& path,
                       const ui::Brush& brush,
                       ui::StrokeStyle style,
                       ui::PaintOptions options) {
    painter.stroke_rounded_rect(rect, 2.0f, 1.0f, brush, options);
    painter.line(a, b, 1.0f, brush, options);
    painter.arc(a, 8.0f, 0.0f, 1.0f, 1.0f, brush, options);
    painter.stroke_path(path, brush, style, options);
});

void nativeui_header_compile_paint() {}
