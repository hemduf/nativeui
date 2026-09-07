#include <nativeui/paint_style.hpp>

void nativeui_header_compile_paint_style() {
    const ui::LinearGradient linear{
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
    const ui::RadialGradient radial{
        {0.0f, 0.0f}, 1.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
    const ui::PaintOptions options{0.5f, ui::BlendMode::Multiply};
    (void)linear;
    (void)radial;
    (void)options;
}
