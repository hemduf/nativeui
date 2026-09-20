#include "test_support.hpp"
#include "golden/golden.hpp"

#include "src/detail/image_texture_brush_access.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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

constexpr std::array<std::byte, 84> kCropIsolationPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{127},
    std::byte{168}, std::byte{125}, std::byte{99}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{27}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{156}, std::byte{93}, std::byte{193}, std::byte{177}, std::byte{1}, std::byte{0},
    std::byte{48}, std::byte{12}, std::byte{195}, std::byte{32}, std::byte{220}, std::byte{255},
    std::byte{127}, std::byte{86}, std::byte{246}, std::byte{194}, std::byte{82}, std::byte{12},
    std::byte{172}, std::byte{60}, std::byte{159}, std::byte{3}, std::byte{133}, std::byte{17},
    std::byte{5}, std::byte{1}, std::byte{100}, std::byte{48}, std::byte{117}, std::byte{66},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{73}, std::byte{69},
    std::byte{78}, std::byte{68}, std::byte{174}, std::byte{66}, std::byte{96}, std::byte{130},
};

[[nodiscard]] bool rect_equal(ui::Rect a, ui::Rect b) noexcept {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

[[nodiscard]] bool zero_rect(ui::Rect value) noexcept {
    return rect_equal(value, {});
}

[[nodiscard]] bool red_dominant(ui::Rgba8 pixel) noexcept {
    return pixel.r > 170 && pixel.r > pixel.g * 2 && pixel.r > pixel.b * 2;
}

[[nodiscard]] bool green_dominant(ui::Rgba8 pixel) noexcept {
    return pixel.g > 170 && pixel.g > pixel.r * 2 && pixel.g > pixel.b * 2;
}

void api_and_validation_contract() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const ui::ImageTexture whole{image, {-4.0f, 3.0f, 16.0f, 8.0f}};
    NUI_CHECK(whole.valid());
    NUI_CHECK(whole.image() == image);
    NUI_CHECK(rect_equal(whole.source(), {0.0f, 0.0f, 2.0f, 2.0f}));
    NUI_CHECK(rect_equal(whole.destination(), {-4.0f, 3.0f, 16.0f, 8.0f}));

    const ui::ImageTexture fractional{
        image, {0.25f, 0.25f, 1.5f, 1.5f}, {1.0f, 2.0f, 12.0f, 10.0f}};
    NUI_CHECK(fractional.valid());
    NUI_CHECK(rect_equal(fractional.source(), {0.25f, 0.25f, 1.5f, 1.5f}));
    NUI_CHECK(rect_equal(fractional.destination(), {1.0f, 2.0f, 12.0f, 10.0f}));

    const std::array<ui::Rect, 5> touching{
        ui::Rect{0.0f, 0.0f, 2.0f, 2.0f},
        ui::Rect{0.0f, 0.5f, 1.0f, 1.0f},
        ui::Rect{1.0f, 0.5f, 1.0f, 1.0f},
        ui::Rect{0.5f, 0.0f, 1.0f, 1.0f},
        ui::Rect{0.5f, 1.0f, 1.0f, 1.0f},
    };
    for (const auto source : touching) {
        const ui::ImageTexture boundary{
            image, source, {0.0f, 0.0f, 8.0f, 8.0f}};
        NUI_CHECK(boundary.valid());
    }

    const std::array<ui::Rect, 4> outside{
        ui::Rect{-0.001f, 0.0f, 1.0f, 1.0f},
        ui::Rect{1.001f, 0.0f, 1.0f, 1.0f},
        ui::Rect{0.0f, -0.001f, 1.0f, 1.0f},
        ui::Rect{0.0f, 1.001f, 1.0f, 1.0f},
    };
    for (const auto source : outside) {
        const ui::ImageTexture invalid{
            image, source, {0.0f, 0.0f, 8.0f, 8.0f}};
        NUI_CHECK(!invalid.valid());
        NUI_CHECK(!invalid.image().valid());
        NUI_CHECK(zero_rect(invalid.source()));
        NUI_CHECK(zero_rect(invalid.destination()));
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const std::array<ui::ImageTexture, 10> invalid_values{
        ui::ImageTexture{},
        ui::ImageTexture{ui::Image{}, {0.0f, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {nan, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, inf, 1.0f, 1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 1.0f, 1.0f}, {nan, 0.0f, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, inf, 8.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 8.0f}},
        ui::ImageTexture{image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 8.0f, -1.0f}},
    };
    for (const auto& invalid : invalid_values) {
        NUI_CHECK(!invalid.valid());
        NUI_CHECK(!invalid.image().valid());
        NUI_CHECK(zero_rect(invalid.source()));
        NUI_CHECK(zero_rect(invalid.destination()));
    }

    ui::ImageTexture moved_source{image, {0.0f, 0.0f, 8.0f, 8.0f}};
    ui::ImageTexture moved{std::move(moved_source)};
    NUI_CHECK(moved.valid());
    NUI_CHECK(!moved_source.valid());
    NUI_CHECK(zero_rect(moved_source.source()));
    NUI_CHECK(zero_rect(moved_source.destination()));

    auto* alias = &moved;
    moved = std::move(*alias);
    NUI_CHECK(!moved.valid());
    NUI_CHECK(zero_rect(moved.source()));
    NUI_CHECK(zero_rect(moved.destination()));
}

void whole_image_mapping_golden() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    const ui::Brush brush{ui::ImageTexture{image, {0.0f, 0.0f, 16.0f, 16.0f}}};

    ui::UI tree{ui::Canvas{16.0f, 16.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 16.0f, 16.0f}, brush);
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    test::golden::Image expected{
        16, 16, std::vector<std::uint8_t>(16U * 16U * 3U, 0U)};
    const auto fill = [&expected](test::golden::Region region,
                                  std::array<std::uint8_t, 3> rgb) {
        for (int y = region.y; y < region.y + region.h; ++y) {
            for (int x = region.x; x < region.x + region.w; ++x) {
                const auto offset =
                    (static_cast<std::size_t>(y) * 16U +
                     static_cast<std::size_t>(x)) * 3U;
                expected.rgb[offset + 0] = rgb[0];
                expected.rgb[offset + 1] = rgb[1];
                expected.rgb[offset + 2] = rgb[2];
            }
        }
    };

    const std::array<test::golden::Region, 4> regions{
        test::golden::Region{0, 0, 3, 3},
        test::golden::Region{13, 0, 3, 3},
        test::golden::Region{0, 13, 3, 3},
        test::golden::Region{13, 13, 3, 3},
    };
    fill(regions[0], {255, 0, 0});
    fill(regions[1], {0, 255, 0});
    fill(regions[2], {0, 0, 255});
    fill(regions[3], {255, 255, 255});

    test::golden::CompareOptions options;
    options.channel_tolerance = 3;
    options.compare_regions.assign(regions.begin(), regions.end());
    const auto result =
        test::golden::compare(expected, test::golden::from_renderer(renderer), options);
    NUI_CHECK(result.matched);
    NUI_CHECK(result.compared_pixels == 36U);
}

void source_subrect_isolation_golden() {
    const auto image = ui::Image::decode(kCropIsolationPng);
    NUI_CHECK(image.valid());
    NUI_CHECK(image.size().w == 4.0f && image.size().h == 2.0f);

    const ui::ImageTexture exact_crop{
        image, {1.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 8.0f}};
    const ui::Brush brush{exact_crop};

    ui::UI tree{ui::Canvas{16.0f, 8.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, brush);
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    test::golden::Image expected{
        16, 8, std::vector<std::uint8_t>(16U * 8U * 3U, 0U)};
    for (std::size_t i = 0; i < 16U * 8U; ++i) {
        expected.rgb[i * 3U] = 255U;
    }
    test::golden::CompareOptions options;
    options.channel_tolerance = 2;
    const auto result =
        test::golden::compare(expected, test::golden::from_renderer(renderer), options);
    NUI_CHECK(result.matched);
    NUI_CHECK(result.compared_pixels == 128U);

    const ui::ImageTexture fractional_crop{
        image, {1.25f, 0.0f, 1.5f, 2.0f}, {0.0f, 0.0f, 12.0f, 8.0f}};
    NUI_CHECK(fractional_crop.valid());
    ui::UI fractional_tree{
        ui::Canvas{12.0f, 8.0f, [fractional = ui::Brush{fractional_crop}](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 12.0f, 8.0f}, fractional);
        }}
    };
    ui::HeadlessRenderer fractional_renderer{{12.0f, 8.0f}, 1.0f};
    NUI_CHECK(fractional_renderer.render(fractional_tree));
    NUI_CHECK(red_dominant(fractional_renderer.pixel(1, 4)));
    NUI_CHECK(red_dominant(fractional_renderer.pixel(10, 4)));
}

void fractional_source_mapping_matches_strict_draw_image() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    constexpr ui::Rect source{0.25f, 0.25f, 1.5f, 1.5f};
    constexpr ui::Rect destination{0.0f, 0.0f, 12.0f, 10.0f};

    ui::UI oracle_tree{
        ui::Canvas{12.0f, 10.0f, [image](ui::CanvasContext2D& g) {
            g.draw_image(image, source, destination);
        }}
    };
    ui::HeadlessRenderer oracle{{12.0f, 10.0f}, 1.0f};
    NUI_CHECK(oracle.render(oracle_tree));

    const ui::Brush texture{
        ui::ImageTexture{image, source, destination}};
    ui::UI texture_tree{
        ui::Canvas{12.0f, 10.0f, [texture](ui::CanvasContext2D& g) {
            g.fill_rect(destination, texture);
        }}
    };
    ui::HeadlessRenderer rendered{{12.0f, 10.0f}, 1.0f};
    NUI_CHECK(rendered.render(texture_tree));

    test::golden::CompareOptions options;
    options.channel_tolerance = 2;
    const auto result = test::golden::compare(
        test::golden::from_renderer(oracle),
        test::golden::from_renderer(rendered),
        options);
    NUI_CHECK(result.matched);
    NUI_CHECK(result.compared_pixels == 120U);
}

void painter_local_mapping_transform_and_stroke() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    const ui::Brush brush{ui::ImageTexture{image, {0.0f, 0.0f, 16.0f, 8.0f}}};

    ui::UI split_tree{ui::Canvas{16.0f, 8.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, brush);
        g.fill_rect({8.0f, 0.0f, 8.0f, 8.0f}, brush);
    }}};
    ui::HeadlessRenderer split{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(split.render(split_tree));
    NUI_CHECK(red_dominant(split.pixel(2, 1)));
    NUI_CHECK(green_dominant(split.pixel(13, 1)));

    ui::UI transformed_tree{
        ui::Canvas{32.0f, 8.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, brush);
            g.save();
            g.translate(16.0f, 0.0f);
            g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, brush);
            g.restore();
        }}
    };
    ui::HeadlessRenderer transformed{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(transformed.render(transformed_tree));
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            const auto a = transformed.pixel(x, y);
            const auto b = transformed.pixel(x + 16, y);
            NUI_CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
        }
    }

    ui::Path line;
    line.move_to({0.0f, 2.0f}).line_to({16.0f, 2.0f});
    ui::UI stroke_tree{ui::Canvas{16.0f, 5.0f, [brush, line](ui::CanvasContext2D& g) {
        g.stroke_path(
            line,
            brush,
            ui::StrokeStyle{3.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 4.0f});
    }}};
    ui::HeadlessRenderer stroke{{16.0f, 5.0f}, 1.0f};
    NUI_CHECK(stroke.render(stroke_tree));
    NUI_CHECK(red_dominant(stroke.pixel(2, 2)));
    NUI_CHECK(green_dominant(stroke.pixel(13, 2)));
}

void paint_options_apply_once() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    const ui::Brush white{ui::ImageTexture{
        image, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 8.0f, 8.0f}}};
    const ui::Brush red{ui::ImageTexture{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {8.0f, 0.0f, 8.0f, 8.0f}}};

    ui::UI tree{ui::Canvas{16.0f, 8.0f, [white, red](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        g.fill_rect(
            {0.0f, 0.0f, 8.0f, 8.0f},
            white,
            ui::PaintOptions{0.25f, ui::BlendMode::SourceOver});

        g.fill_rect({8.0f, 0.0f, 8.0f, 8.0f}, {0.8f, 0.5f, 0.25f, 1.0f});
        g.fill_rect(
            {8.0f, 0.0f, 8.0f, 8.0f},
            red,
            ui::PaintOptions{1.0f, ui::BlendMode::Multiply});
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto translucent = renderer.pixel(4, 4);
    NUI_CHECK(translucent.r > 50 && translucent.r < 80);
    NUI_CHECK(translucent.g > 50 && translucent.g < 80);
    NUI_CHECK(translucent.b > 50 && translucent.b < 80);

    const auto multiplied = renderer.pixel(12, 4);
    NUI_CHECK(multiplied.r > 170);
    NUI_CHECK(multiplied.g < 20);
    NUI_CHECK(multiplied.b < 20);
}

class TestResourceProvider final : public ui::ResourceProvider {
public:
    std::optional<std::vector<std::byte>> load(std::string_view resource_id) override {
        ++loads;
        if (resource_id != "texture") return std::nullopt;
        return std::vector<std::byte>{kTinyRgbaPng.begin(), kTinyRgbaPng.end()};
    }

    int loads{};
};

void lifetime_sharing_and_two_renderer_isolation() {
    ui::Brush survivor = [] {
        TestResourceProvider provider;
        ui::ImageCache cache{provider};
        const auto loaded = cache.load("texture");
        NUI_CHECK(loaded);
        NUI_CHECK(provider.loads == 1);
        return ui::Brush{
            ui::ImageTexture{loaded.image, {0.0f, 0.0f, 16.0f, 8.0f}}};
    }();

    const auto image = ui::Image::decode(kTinyRgbaPng);
    const ui::ImageTexture first_texture{image, {0.0f, 0.0f, 16.0f, 8.0f}};
    const ui::ImageTexture second_texture{image, {0.0f, 0.0f, 16.0f, 8.0f}};
    NUI_CHECK(first_texture.image() == second_texture.image());

    const ui::Brush first_brush{first_texture};
    const ui::Brush second_brush{second_texture};
    const auto* first_stored = ui::detail::ImageTextureBrushAccess::texture(first_brush);
    const auto* second_stored = ui::detail::ImageTextureBrushAccess::texture(second_brush);
    NUI_CHECK(first_stored != nullptr && second_stored != nullptr);
    NUI_CHECK(first_stored->image() == second_stored->image());

    auto make_tree = [survivor] {
        return std::make_unique<ui::UI>(
            ui::Canvas{16.0f, 8.0f, [survivor](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, survivor);
            }});
    };

    auto second_tree = make_tree();
    ui::HeadlessRenderer second{{16.0f, 8.0f}, 1.0f};
    {
        auto first_tree = make_tree();
        ui::HeadlessRenderer first{{16.0f, 8.0f}, 1.0f};
        NUI_CHECK(first.render(*first_tree));
        NUI_CHECK(red_dominant(first.pixel(1, 1)));
        NUI_CHECK(second.render(*second_tree));
    }

    NUI_CHECK(second.render(*second_tree));
    NUI_CHECK(red_dominant(second.pixel(1, 1)));
    NUI_CHECK(green_dominant(second.pixel(14, 1)));
}

void shader_child_materializes_image_texture() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto program = ui::ShaderProgram::compile(R"(
        uniform shader source;
        half4 main(float2 p) {
            return source.eval(p);
        }
    )");
    NUI_CHECK(program.ok());

    ui::ShaderInstance instance{program.program};
    const ui::Brush texture{
        ui::ImageTexture{image, {0.0f, 0.0f, 16.0f, 8.0f}}};
    NUI_CHECK(instance.set_child("source", texture) == ui::ShaderSetResult::Ok);

    const ui::Brush shader{instance};
    ui::UI tree{ui::Canvas{16.0f, 8.0f, [shader](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, shader);
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(2, 1)));
    NUI_CHECK(green_dominant(renderer.pixel(13, 1)));
}

void brush_value_and_invalid_contract() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    ui::Brush source{ui::ImageTexture{image, {0.0f, 0.0f, 16.0f, 8.0f}}};
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_image_texture(source));

    ui::Brush copied{source};
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_image_texture(copied));
    const auto* copied_texture = ui::detail::ImageTextureBrushAccess::texture(copied);
    NUI_CHECK(copied_texture != nullptr && copied_texture->image() == image);

    ui::Brush moved{std::move(copied)};
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_image_texture(moved));
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_transparent_solid(copied));

    ui::Brush assigned{ui::Color{0.0f, 1.0f, 0.0f, 1.0f}};
    assigned = source;
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_image_texture(assigned));

    auto* alias = &moved;
    moved = std::move(*alias);
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_transparent_solid(moved));

    const ui::Brush invalid{ui::ImageTexture{}};
    NUI_CHECK(ui::detail::ImageTextureBrushAccess::is_transparent_solid(invalid));

    ui::UI tree{ui::Canvas{8.0f, 8.0f, [invalid](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
        g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, invalid);
    }}};
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(4, 4)));
}

void suite() {
    api_and_validation_contract();
    whole_image_mapping_golden();
    source_subrect_isolation_golden();
    fractional_source_mapping_matches_strict_draw_image();
    painter_local_mapping_transform_and_stroke();
    paint_options_apply_once();
    lifetime_sharing_and_two_renderer_isolation();
    shader_child_materializes_image_texture();
    brush_value_and_invalid_contract();
}

} // namespace

int main() {
    return test::run("T083 ImageTexture Brush", &suite);
}
