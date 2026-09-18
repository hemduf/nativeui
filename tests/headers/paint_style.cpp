#include <nativeui/paint_style.hpp>

#include <type_traits>

void nativeui_header_compile_paint_style() {
    const ui::LinearGradient linear{
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
    const ui::RadialGradient radial{
        {0.0f, 0.0f}, 1.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
    const ui::PaintOptions options{0.5f, ui::BlendMode::Multiply};
    const ui::Effect blur = ui::Effect::gaussian_blur(2.0f, 3.0f);
    const ui::Effect shadow =
        ui::Effect::drop_shadow({4.0f, 6.0f}, 3.0f, {0.0f, 0.0f, 0.0f, 0.5f});
    const ui::Effect shadow_only =
        ui::Effect::drop_shadow_only({-4.0f, 2.0f}, 2.0f, {0.0f, 0.0f, 0.0f, 0.5f});
    const ui::VisualOutset outset = shadow.visual_outset();
    static_assert(noexcept(ui::Effect::gaussian_blur(1.0f, 1.0f)));
    static_assert(noexcept(ui::Effect::drop_shadow(ui::Point{}, 1.0f, ui::Color{})));
    static_assert(noexcept(ui::Effect::drop_shadow_only(ui::Point{}, 1.0f, ui::Color{})));
    static_assert(noexcept(std::declval<const ui::Effect&>().visual_outset()));
    static_assert(noexcept(ui::VisualOutset::uniform(1.0f)));
    static_assert(std::is_nothrow_copy_constructible_v<ui::Effect>);
    static_assert(std::is_nothrow_copy_assignable_v<ui::Effect>);
    static_assert(std::is_nothrow_move_constructible_v<ui::Effect>);
    static_assert(std::is_nothrow_move_assignable_v<ui::Effect>);
    static_assert(std::is_nothrow_destructible_v<ui::Effect>);
    const ui::Brush solid{{0.25f, 0.5f, 0.75f, 1.0f}};
    const ui::Brush linear_brush{linear};
    const ui::Brush radial_brush{radial};
    (void)linear;
    (void)radial;
    (void)options;
    (void)blur;
    (void)shadow;
    (void)shadow_only;
    (void)outset;
    (void)solid;
    (void)linear_brush;
    (void)radial_brush;
}
