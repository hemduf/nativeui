#include "example_support.hpp"

#include <nativeui/headless.hpp>

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

int self_test() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return example::fail("ImageTexture sample image did not decode");

    const ui::Brush brush{
        ui::ImageTexture{image, {0.0f, 0.0f, 32.0f, 16.0f}}};

    ui::UI tree{ui::Canvas{32.0f, 16.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, brush);
    }}};
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("ImageTexture headless render failed");

    const auto red = renderer.pixel(2, 2);
    const auto green = renderer.pixel(29, 2);
    const auto blue = renderer.pixel(2, 13);
    const auto white = renderer.pixel(29, 13);
    if (!(red.r > 180 && red.g < 70 && red.b < 70 &&
          green.g > 180 && green.r < 70 && green.b < 70 &&
          blue.b > 180 && blue.r < 70 && blue.g < 70 &&
          white.r > 180 && white.g > 180 && white.b > 180)) {
        return example::fail("ImageTexture mapping did not preserve image corners");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return 1;

    const ui::Brush texture{
        ui::ImageTexture{image, {32.0f, 24.0f, 256.0f, 132.0f}}};

    ui::UI tree{ui::Canvas{320.0f, 180.0f, [texture](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 320.0f, 180.0f}, {0.06f, 0.06f, 0.07f, 1.0f});
        g.fill_rounded_rect({32.0f, 24.0f, 256.0f, 132.0f}, 18.0f, texture);
        g.stroke_rounded_rect(
            {44.0f, 36.0f, 232.0f, 108.0f}, 14.0f, 5.0f, texture);
    }}};
    return example::run_window(
        tree, "NativeUI T083 ImageTexture Brush", {320.0f, 180.0f});
}
