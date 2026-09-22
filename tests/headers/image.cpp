#include <nativeui/image.hpp>

#include <type_traits>

static_assert(noexcept(ui::ImageTexture{}));
static_assert(std::is_nothrow_copy_constructible_v<ui::ImageTexture>);
static_assert(std::is_nothrow_copy_assignable_v<ui::ImageTexture>);
static_assert(std::is_nothrow_move_constructible_v<ui::ImageTexture>);
static_assert(std::is_nothrow_move_assignable_v<ui::ImageTexture>);
static_assert(std::is_nothrow_destructible_v<ui::ImageTexture>);

void nativeui_header_compile_image() {
    ui::Image image;
    const ui::ImageTexture whole{image, {0.0f, 0.0f, 8.0f, 8.0f}};
    const ui::ImageTexture crop{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}};
    static_assert(noexcept(ui::ImageTexture{ui::Image{}, ui::Rect{}}));
    static_assert(noexcept(ui::ImageTexture{ui::Image{}, ui::Rect{}, ui::Rect{}}));
    static_assert(noexcept(whole.valid()));
    static_assert(noexcept(whole.image()));
    static_assert(noexcept(whole.source()));
    static_assert(noexcept(whole.destination()));
    (void)whole;
    (void)crop;
}
