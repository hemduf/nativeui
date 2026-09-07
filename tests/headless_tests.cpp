#include "test_support.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::array<std::byte, 75> kTinyRgbaPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{114},
    std::byte{182}, std::byte{13}, std::byte{36}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{18}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{129}, std::byte{52}, std::byte{24}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{200}, std::byte{9}, std::byte{247}, std::byte{3},
    std::byte{217}, std::byte{100}, std::byte{241}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68}, std::byte{174},
    std::byte{66}, std::byte{96}, std::byte{130},
};

void in_memory_image_decode_and_draw() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    NUI_CHECK(image.size().w == 2.0f);
    NUI_CHECK(image.size().h == 2.0f);

    ui::UI tree{
        ui::Canvas{8.0f, 8.0f, [image](ui::CanvasContext2D& canvas) {
            canvas.draw_image(image, {0.0f, 0.0f, 8.0f, 8.0f});
        }}
    };

    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto red = renderer.pixel(1, 1);
    const auto green = renderer.pixel(6, 1);
    const auto blue = renderer.pixel(1, 6);
    const auto white = renderer.pixel(6, 6);
    NUI_CHECK(red.r > 200 && red.g < 40 && red.b < 40);
    NUI_CHECK(green.g > 200 && green.r < 40 && green.b < 40);
    NUI_CHECK(blue.b > 200 && blue.r < 40 && blue.g < 40);
    NUI_CHECK(white.r > 200 && white.g > 200 && white.b > 200);
}

void suite() {
    ui::State<bool> enabled{true};
    ui::UI tree{
        ui::Padding{4.0f,
            ui::Toggle{"Enabled", enabled}}
    };

    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 96);
    NUI_CHECK(renderer.pixel_height() == 48);
    NUI_CHECK(renderer.rgba_pixels().size() == 96U * 48U * 4U);

    // NativeUI paints an opaque background, so a successful raster frame must
    // contain non-zero alpha without needing a display server or GL context.
    const auto background = renderer.pixel(0, 0);
    NUI_CHECK(background.a != 0);

    // The same logical surface at 2x produces exactly twice the physical
    // dimensions while keeping UI layout coordinates logical.
    renderer.resize({96.0f, 48.0f}, 2.0f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 192);
    NUI_CHECK(renderer.pixel_height() == 96);
    NUI_CHECK(renderer.rgba_pixels().size() == 192U * 96U * 4U);
    NUI_CHECK(renderer.pixel(0, 0).a != 0);

    // A state-only repaint is consumable headlessly without a layout invalidation.
    enabled.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    in_memory_image_decode_and_draw();
}

} // namespace

int main() { return test::run("headless", &suite); }
