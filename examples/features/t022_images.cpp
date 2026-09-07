#include "example_support.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::array<std::byte, 75> kDemoPng{
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

bool quadrants_rendered(const ui::HeadlessRenderer& renderer) {
    const auto red = renderer.pixel(2, 2);
    const auto green = renderer.pixel(13, 2);
    const auto blue = renderer.pixel(2, 13);
    const auto white = renderer.pixel(13, 13);
    return red.r > 200 && red.g < 40 && red.b < 40 &&
           green.g > 200 && green.r < 40 && green.b < 40 &&
           blue.b > 200 && blue.r < 40 && blue.g < 40 &&
           white.r > 200 && white.g > 200 && white.b > 200;
}

} // namespace

int main(int argc, char** argv) {
    const auto image = ui::Image::decode(kDemoPng);
    if (!image.valid()) return example::fail("embedded image decode failed");

    if (example::self_test_requested(argc, argv)) {
        ui::UI tree{ui::Canvas{16.0f, 16.0f, [image](ui::CanvasContext2D& g) {
            g.draw_image(image, {0.0f, 0.0f, 16.0f, 16.0f});
        }}};
        ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless image render failed");
        if (!quadrants_rendered(renderer)) return example::fail("scaled image quadrants changed");
        return 0;
    }

    auto tree = std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T022 / IMAGE RESOURCES"},
            ui::Canvas{500.0f, 260.0f, [image](ui::CanvasContext2D& g) {
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                g.draw_image(image, {20.0f, 30.0f, 130.0f, 190.0f}, ui::ImageFit::Fill);
                g.draw_image(image, {185.0f, 30.0f, 130.0f, 190.0f}, ui::ImageFit::Contain);
                g.draw_image(image, {350.0f, 30.0f, 130.0f, 190.0f}, ui::ImageFit::Cover);
                g.text({20.0f, 244.0f}, "Fill", 11.0f, ui::colors::textMuted);
                g.text({185.0f, 244.0f}, "Contain", 11.0f, ui::colors::textMuted);
                g.text({350.0f, 244.0f}, "Cover", 11.0f, ui::colors::textMuted);
            }}
        }.padding(20.0f).gap(12.0f));
    return example::run_window(*tree, "NativeUI T022 - Image Resources", {580.0f, 430.0f});
}
