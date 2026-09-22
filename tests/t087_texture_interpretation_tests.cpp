#include "test_support.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace {

constexpr std::array<std::byte, 130> kLinearTaggedPng{
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

constexpr std::array<std::byte, 70> kUntaggedPng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{1},std::byte{0},std::byte{0},std::byte{0},std::byte{1},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{31},std::byte{21},std::byte{196},
    std::byte{137},std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{68},std::byte{65},
    std::byte{84},std::byte{120},std::byte{218},std::byte{99},std::byte{104},std::byte{112},std::byte{80},std::byte{248},
    std::byte{15},std::byte{0},std::byte{4},std::byte{4},std::byte{1},std::byte{224},std::byte{45},std::byte{181},
    std::byte{146},std::byte{233},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{73},std::byte{69},
    std::byte{78},std::byte{68},std::byte{174},std::byte{66},std::byte{96},std::byte{130},
};

constexpr std::array<std::byte, 70> kAlphaPayloadPng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{1},std::byte{0},std::byte{0},std::byte{0},std::byte{1},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{31},std::byte{21},std::byte{196},
    std::byte{137},std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{68},std::byte{65},
    std::byte{84},std::byte{120},std::byte{218},std::byte{99},std::byte{56},std::byte{145},std::byte{98},std::byte{228},
    std::byte{0},std::byte{0},std::byte{4},std::byte{245},std::byte{1},std::byte{159},std::byte{91},std::byte{144},
    std::byte{228},std::byte{44},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{73},std::byte{69},
    std::byte{78},std::byte{68},std::byte{174},std::byte{66},std::byte{96},std::byte{130},
};

constexpr std::array<std::byte, 74> kNormalLikePng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{2},std::byte{0},std::byte{0},std::byte{0},std::byte{1},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{244},std::byte{34},std::byte{127},
    std::byte{138},std::byte{0},std::byte{0},std::byte{0},std::byte{17},std::byte{73},std::byte{68},std::byte{65},
    std::byte{84},std::byte{120},std::byte{218},std::byte{99},std::byte{104},std::byte{112},std::byte{248},std::byte{255},
    std::byte{95},std::byte{225},std::byte{68},std::byte{194},std::byte{127},std::byte{0},std::byte{21},std::byte{85},
    std::byte{5},std::byte{6},std::byte{109},std::byte{89},std::byte{166},std::byte{137},std::byte{0},std::byte{0},
    std::byte{0},std::byte{0},std::byte{73},std::byte{69},std::byte{78},std::byte{68},std::byte{174},std::byte{66},
    std::byte{96},std::byte{130},
};

constexpr std::array<std::byte, 135> kMipTaggedPng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{4},std::byte{0},std::byte{0},std::byte{0},std::byte{4},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{169},std::byte{241},std::byte{158},
    std::byte{126},std::byte{0},std::byte{0},std::byte{0},std::byte{4},std::byte{103},std::byte{65},std::byte{77},
    std::byte{65},std::byte{0},std::byte{1},std::byte{134},std::byte{160},std::byte{49},std::byte{232},std::byte{150},
    std::byte{95},std::byte{0},std::byte{0},std::byte{0},std::byte{32},std::byte{99},std::byte{72},std::byte{82},
    std::byte{77},std::byte{0},std::byte{0},std::byte{122},std::byte{38},std::byte{0},std::byte{0},std::byte{128},
    std::byte{132},std::byte{0},std::byte{0},std::byte{250},std::byte{0},std::byte{0},std::byte{0},std::byte{128},
    std::byte{232},std::byte{0},std::byte{0},std::byte{117},std::byte{48},std::byte{0},std::byte{0},std::byte{234},
    std::byte{96},std::byte{0},std::byte{0},std::byte{58},std::byte{152},std::byte{0},std::byte{0},std::byte{23},
    std::byte{112},std::byte{156},std::byte{186},std::byte{81},std::byte{60},std::byte{0},std::byte{0},std::byte{0},
    std::byte{18},std::byte{73},std::byte{68},std::byte{65},std::byte{84},std::byte{120},std::byte{218},std::byte{99},
    std::byte{104},std::byte{112},std::byte{80},std::byte{248},std::byte{143},std::byte{140},std::byte{25},std::byte{72},
    std::byte{23},std::byte{0},std::byte{0},std::byte{239},std::byte{105},std::byte{29},std::byte{241},std::byte{81},
    std::byte{243},std::byte{14},std::byte{61},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{73},
    std::byte{69},std::byte{78},std::byte{68},std::byte{174},std::byte{66},std::byte{96},std::byte{130},
};

struct Pixel {
    int r{};
    int g{};
    int b{};
    int a{};
};

[[nodiscard]] Pixel render_brush(const ui::Brush& brush, int x = 8, int y = 8) {
    const auto info = SkImageInfo::Make(
        16, 16, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
        SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface);
    surface->getCanvas()->clear(SK_ColorBLACK);
    ui::Painter painter{*surface->getCanvas()};
    painter.fill_rounded_rect({0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, brush);

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap));
    const auto color = pixmap.getColor(x, y);
    return {
        SkColorGetR(color),
        SkColorGetG(color),
        SkColorGetB(color),
        SkColorGetA(color),
    };
}

[[nodiscard]] bool near_channel(int actual, int expected, int tolerance = 3) noexcept {
    return std::abs(actual - expected) <= tolerance;
}

[[nodiscard]] ui::Brush opaque_sample(const ui::ImageTexture& texture) {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader source;
        half4 main(float2 p) {
            half4 value = source.eval(p);
            return half4(value.rgb, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_child("source", ui::Brush{texture}) ==
              ui::ShaderSetResult::Ok);
    return ui::Brush{instance};
}

[[nodiscard]] ui::Brush alpha_as_rgb(const ui::ImageTexture& texture) {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader source;
        half4 main(float2 p) {
            half4 value = source.eval(p);
            return half4(value.aaa, 1.0);
        }
    )");
    NUI_CHECK(compiled.ok());
    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_child("source", ui::Brush{texture}) ==
              ui::ShaderSetResult::Ok);
    return ui::Brush{instance};
}

void api_and_value_semantics() {
    static_assert(noexcept(std::declval<ui::ImageTexture&>().set_interpretation(
        ui::TextureInterpretation::Data)));
    static_assert(noexcept(std::declval<const ui::ImageTexture&>().interpretation()));
    static_assert(std::is_same_v<
        decltype(std::declval<ui::ImageTexture&>().set_interpretation(
            ui::TextureInterpretation::Data)),
        ui::ImageTexture&>);

    const auto image = ui::Image::decode(kUntaggedPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    NUI_CHECK(texture.interpretation() == ui::TextureInterpretation::Color);

    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Nearest)
        .set_mipmap(ui::TextureMipmap::None);
    texture.set_tile_mode(ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror)
        .set_sampling(sampling)
        .set_transform(ui::Transform2D::translation(2.0f, 3.0f));

    auto data = texture;
    NUI_CHECK(&data.set_interpretation(ui::TextureInterpretation::Data) == &data);
    NUI_CHECK(texture.interpretation() == ui::TextureInterpretation::Color);
    NUI_CHECK(data.interpretation() == ui::TextureInterpretation::Data);
    NUI_CHECK(data.tile_mode_x() == texture.tile_mode_x());
    NUI_CHECK(data.tile_mode_y() == texture.tile_mode_y());
    NUI_CHECK(data.sampling().filter() == texture.sampling().filter());
    NUI_CHECK(data.sampling().mipmap() == texture.sampling().mipmap());
    NUI_CHECK(data.transform().m02 == texture.transform().m02);
    NUI_CHECK(data.transform().m12 == texture.transform().m12);

    ui::ImageTexture moved{std::move(data)};
    NUI_CHECK(moved.interpretation() == ui::TextureInterpretation::Data);
    NUI_CHECK(data.interpretation() == ui::TextureInterpretation::Color);

    auto* alias = &moved;
    moved = std::move(*alias);
    NUI_CHECK(!moved.valid());
    NUI_CHECK(moved.interpretation() == ui::TextureInterpretation::Color);
}

void tagged_color_and_data_diverge() {
    const auto image = ui::Image::decode(kLinearTaggedPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    const auto color_pixel = render_brush(ui::Brush{color});
    const auto data_pixel = render_brush(ui::Brush{data});

    NUI_CHECK(near_channel(data_pixel.r, 128));
    NUI_CHECK(near_channel(data_pixel.g, 64));
    NUI_CHECK(near_channel(data_pixel.b, 32));
    NUI_CHECK(color_pixel.r > data_pixel.r + 35);
    NUI_CHECK(color_pixel.g > data_pixel.g + 25);
}

void untagged_color_defaults_to_srgb() {
    const auto image = ui::Image::decode(kUntaggedPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    const auto color_pixel = render_brush(ui::Brush{color});
    const auto data_pixel = render_brush(ui::Brush{data});
    NUI_CHECK(near_channel(color_pixel.r, data_pixel.r));
    NUI_CHECK(near_channel(color_pixel.g, data_pixel.g));
    NUI_CHECK(near_channel(color_pixel.b, data_pixel.b));
}

void data_preserves_rgb_and_alpha_payload() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    const auto raw_rgb = render_brush(opaque_sample(data));
    NUI_CHECK(near_channel(raw_rgb.r, 200));
    NUI_CHECK(near_channel(raw_rgb.g, 100));
    NUI_CHECK(near_channel(raw_rgb.b, 50));

    const auto raw_alpha = render_brush(alpha_as_rgb(data));
    NUI_CHECK(near_channel(raw_alpha.r, 64));
    NUI_CHECK(near_channel(raw_alpha.g, 64));
    NUI_CHECK(near_channel(raw_alpha.b, 64));

    const auto color_rgb = render_brush(opaque_sample(color));
    NUI_CHECK(color_rgb.r < raw_rgb.r - 80);
    NUI_CHECK(color_rgb.g < raw_rgb.g - 35);
}

void normals_and_shared_image_are_independent() {
    const auto image = ui::Image::decode(kNormalLikePng);
    NUI_CHECK(image.valid());

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);
    ui::TextureSampling nearest;
    nearest.set_filter(ui::TextureFilter::Nearest);
    data.set_sampling(nearest);

    const auto left = render_brush(ui::Brush{data}, 4, 8);
    const auto right = render_brush(ui::Brush{data}, 12, 8);
    NUI_CHECK(near_channel(left.r, 128));
    NUI_CHECK(near_channel(left.g, 64));
    NUI_CHECK(near_channel(left.b, 255));
    NUI_CHECK(near_channel(right.r, 32));
    NUI_CHECK(near_channel(right.g, 200));
    NUI_CHECK(near_channel(right.b, 96));

    NUI_CHECK(color.image() == data.image());
    NUI_CHECK(color.interpretation() == ui::TextureInterpretation::Color);
    NUI_CHECK(data.interpretation() == ui::TextureInterpretation::Data);
}

void brush_snapshot_and_transform_state_are_stable() {
    const auto image = ui::Image::decode(kLinearTaggedPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_transform(
        ui::Transform2D::translation(2.0f, 3.0f) *
        ui::Transform2D::scaling(0.75f, 1.25f));
    const auto before = texture.transform();
    const ui::Brush color_snapshot{texture};

    texture.set_interpretation(ui::TextureInterpretation::Data);
    NUI_CHECK(texture.transform().m00 == before.m00);
    NUI_CHECK(texture.transform().m01 == before.m01);
    NUI_CHECK(texture.transform().m02 == before.m02);
    NUI_CHECK(texture.transform().m10 == before.m10);
    NUI_CHECK(texture.transform().m11 == before.m11);
    NUI_CHECK(texture.transform().m12 == before.m12);

    const auto color_pixel = render_brush(color_snapshot);
    const auto data_pixel = render_brush(ui::Brush{texture});
    NUI_CHECK(color_pixel.r > data_pixel.r + 35);
}

void data_tiling_filtering_and_mipmaps_stay_raw() {
    const auto image = ui::Image::decode(kMipTaggedPng);
    NUI_CHECK(image.valid());

    constexpr std::array<ui::TextureMipmap, 3> mipmaps{
        ui::TextureMipmap::None,
        ui::TextureMipmap::Nearest,
        ui::TextureMipmap::Linear,
    };
    constexpr std::array<ui::TextureFilter, 2> filters{
        ui::TextureFilter::Nearest,
        ui::TextureFilter::Linear,
    };

    for (const auto filter : filters) {
        for (const auto mipmap : mipmaps) {
            ui::ImageTexture texture{
                image, {0.0f, 0.0f, 4.0f, 4.0f}, {4.0f, 4.0f, 4.0f, 4.0f}};
            texture.set_interpretation(ui::TextureInterpretation::Data)
                .set_tile_mode(ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
            ui::TextureSampling sampling;
            sampling.set_filter(filter).set_mipmap(mipmap);
            texture.set_sampling(sampling);

            const auto pixel = render_brush(ui::Brush{texture}, 10, 10);
            NUI_CHECK(near_channel(pixel.r, 128, 4));
            NUI_CHECK(near_channel(pixel.g, 64, 4));
            NUI_CHECK(near_channel(pixel.b, 32, 4));
        }
    }
}

void suite() {
    api_and_value_semantics();
    tagged_color_and_data_diverge();
    untagged_color_defaults_to_srgb();
    data_preserves_rgb_and_alpha_payload();
    normals_and_shared_image_are_independent();
    brush_snapshot_and_transform_state_are_stable();
    data_tiling_filtering_and_mipmaps_stay_raw();
}

} // namespace

int main() {
    return test::run("ImageTexture interpretation", &suite);
}
