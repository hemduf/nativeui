#include "example_support.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace {

constexpr std::array<std::byte, 130> kTaggedPng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{1},std::byte{0},std::byte{0},std::byte{0},std::byte{1},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{31},std::byte{21},std::byte{196},
    std::byte{137},std::byte{0},std::byte{0},std::byte{0},std::byte{4},std::byte{103},std::byte{65},std::byte{77},
    std::byte{65},std::byte{0},std::byte{1},std::byte{134},std::byte{160},std::byte{49},std::byte{232},std::byte{150},
    std::byte{95},std::byte{0},std::byte{0},std::byte{0},std::byte{32},std::byte{99},std::byte{72},std::byte{82},
    std::byte{77},std::byte{0},std::byte{0},std::byte{122},std::byte{38},std::byte{0},std::byte{0},std::byte{128},
    std::byte{132},std::byte{0},std::byte{0},std::byte{250},std::byte{0},std::byte{0},std::byte{0},std::byte{128},
    std::byte{232},std::byte{0},std::byte{0},std::byte{117},std::byte{48},std::byte{0},std::byte{0},std::byte{234},
    std::byte{96},std::byte{0},std::byte{0},std::byte{58},std::byte{152},std::byte{0},std::byte{0},std::byte{23},
    std::byte{112},std::byte{156},std::byte{186},std::byte{81},std::byte{60},std::byte{0},std::byte{0},std::byte{0},
    std::byte{13},std::byte{73},std::byte{68},std::byte{65},std::byte{84},std::byte{120},std::byte{218},std::byte{99},
    std::byte{104},std::byte{112},std::byte{80},std::byte{248},std::byte{15},std::byte{0},std::byte{4},std::byte{4},
    std::byte{1},std::byte{224},std::byte{45},std::byte{181},std::byte{146},std::byte{233},std::byte{0},std::byte{0},
    std::byte{0},std::byte{0},std::byte{73},std::byte{69},std::byte{78},std::byte{68},std::byte{174},std::byte{66},
    std::byte{96},std::byte{130},
};

int self_test() {
    const auto image = ui::Image::decode(kTaggedPng);
    if (!image.valid()) return example::fail("tagged image did not decode");

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {20.0f, 20.0f, 260.0f, 240.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    if (color.interpretation() != ui::TextureInterpretation::Color) {
        return example::fail("Color is not the default interpretation");
    }
    if (data.interpretation() != ui::TextureInterpretation::Data) {
        return example::fail("Data interpretation did not round-trip");
    }
    if (color.image() != data.image()) {
        return example::fail("interpretation unexpectedly replaced shared Image");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    const auto image = ui::Image::decode(kTaggedPng);
    if (!image.valid()) return example::fail("tagged image did not decode");

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {20.0f, 20.0f, 260.0f, 240.0f}};
    ui::ImageTexture data{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {300.0f, 20.0f, 240.0f, 240.0f}};
    data.set_interpretation(ui::TextureInterpretation::Data);
    const ui::Brush color_brush{color};
    const ui::Brush data_brush{data};

    ui::UI tree{ui::Canvas{
        560.0f,
        280.0f,
        [color_brush, data_brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 560.0f, 280.0f}, ui::colors::panel);
            g.fill_rect({20.0f, 20.0f, 260.0f, 240.0f}, color_brush);
            g.fill_rect({300.0f, 20.0f, 240.0f, 240.0f}, data_brush);
        }}};
    return example::run_window(
        tree, "ImageTexture Color vs Data", {560.0f, 280.0f});
}
