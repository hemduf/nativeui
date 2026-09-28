#include <nativeui/component.hpp>

static_assert(requires(ui::CanvasContext2D& canvas,
                       ui::Rect rect,
                       ui::Point a,
                       ui::Point b,
                       const ui::Path& path,
                       const ui::Brush& brush,
                       ui::StrokeStyle style,
                       ui::PaintOptions options) {
    canvas.stroke_rect(rect, 1.0f, brush, options);
    canvas.stroke_rounded_rect(rect, 2.0f, 1.0f, brush, options);
    canvas.line(a, b, 1.0f, brush, options);
    canvas.arc(a, 8.0f, 0.0f, 1.0f, 1.0f, brush, options);
    canvas.stroke_path(path, brush, style, options);
});

static_assert(requires(const ui::Component& component) {
    component.visual_outset();
});

void nativeui_header_compile_component() {}
