#include <nativeui/paint.hpp>

#include <type_traits>
#include <utility>

using NativeUILayerResult = decltype(
    std::declval<ui::Painter&>().scoped_layer(ui::Rect{}, ui::PaintOptions{}));

static_assert(std::is_same_v<NativeUILayerResult, ui::Painter::StateGuard>);
static_assert(!std::is_copy_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_copy_assignable_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_constructible_v<ui::Painter::StateGuard>);
static_assert(!std::is_move_assignable_v<ui::Painter::StateGuard>);
static_assert(std::is_nothrow_destructible_v<ui::Painter::StateGuard>);

static_assert(requires(ui::Painter& painter,
                       ui::Rect rect,
                       ui::Point a,
                       ui::Point b,
                       const ui::Path& path,
                       const ui::Brush& brush,
                       ui::StrokeStyle style,
                       ui::PaintOptions options) {
    painter.scoped_layer(rect, options);
    painter.stroke_rounded_rect(rect, 2.0f, 1.0f, brush, options);
    painter.line(a, b, 1.0f, brush, options);
    painter.arc(a, 8.0f, 0.0f, 1.0f, 1.0f, brush, options);
    painter.stroke_path(path, brush, style, options);
});

void nativeui_header_compile_paint() {}
