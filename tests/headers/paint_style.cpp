#include <nativeui/paint_style.hpp>

void nativeui_header_compile_paint_style() {
    const ui::LinearGradient linear{
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
    const ui::RadialGradient radial{
        {0.0f, 0.0f}, 1.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
    const ui::PaintOptions options{0.5f, ui::BlendMode::Multiply};
    const ui::Brush solid{{0.25f, 0.5f, 0.75f, 1.0f}};
    const ui::Brush linear_brush{linear};
    const ui::Brush radial_brush{radial};
    (void)linear;
    (void)radial;
    (void)options;
    (void)solid;
    (void)linear_brush;
    (void)radial_brush;
}
