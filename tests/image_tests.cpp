#include "test_support.hpp"
#include "golden/golden.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
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

constexpr std::array<std::byte, 78> kWideRgbaPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{127},
    std::byte{168}, std::byte{125}, std::byte{99}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{21}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{25}, std::byte{254}, std::byte{131}, std::byte{1},
    std::byte{3}, std::byte{186}, std::byte{0}, std::byte{0}, std::byte{52}, std::byte{251},
    std::byte{19}, std::byte{237}, std::byte{251}, std::byte{179}, std::byte{56}, std::byte{239},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{73}, std::byte{69},
    std::byte{78}, std::byte{68}, std::byte{174}, std::byte{66}, std::byte{96}, std::byte{130},
};

void decode_and_draw() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    NUI_CHECK(image.size().w == 2.0f);
    NUI_CHECK(image.size().h == 2.0f);

    ui::UI tree{ui::Canvas{8.0f, 8.0f, [image](ui::CanvasContext2D& canvas) {
        canvas.draw_image(image, {0.0f, 0.0f, 8.0f, 8.0f});
    }}};
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

void source_rect_and_fit_modes() {
    const auto image = ui::Image::decode(kWideRgbaPng);
    NUI_CHECK(image.valid());

    ui::UI tree{ui::Canvas{24.0f, 8.0f, [image](ui::CanvasContext2D& canvas) {
        canvas.fill_rect({0.0f, 0.0f, 24.0f, 8.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        canvas.draw_image(image, {0.0f, 0.0f, 1.0f, 2.0f}, {0.0f, 0.0f, 8.0f, 8.0f});
        canvas.draw_image(image, {8.0f, 0.0f, 8.0f, 8.0f}, ui::ImageFit::Contain);
        canvas.draw_image(image, {16.0f, 0.0f, 8.0f, 8.0f}, ui::ImageFit::Cover);
    }}};
    ui::HeadlessRenderer renderer{{24.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto source_red = renderer.pixel(4, 4);
    NUI_CHECK(source_red.r > 220 && source_red.g < 30 && source_red.b < 30);
    const auto contain_bar = renderer.pixel(12, 1);
    NUI_CHECK(contain_bar.r < 15 && contain_bar.g < 15 && contain_bar.b < 15);
    const auto contain_body = renderer.pixel(9, 4);
    NUI_CHECK(contain_body.r > contain_body.g && contain_body.r > contain_body.b);
    const auto cover_left = renderer.pixel(17, 4);
    const auto cover_right = renderer.pixel(22, 4);
    NUI_CHECK(cover_left.g > cover_left.r && cover_left.g > cover_left.b);
    NUI_CHECK(cover_right.b > cover_right.r && cover_right.b > cover_right.g);
}

void scaling_golden() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());
    ui::UI tree{ui::Canvas{16.0f, 16.0f, [image](ui::CanvasContext2D& canvas) {
        canvas.draw_image(image, {0.0f, 0.0f, 16.0f, 16.0f});
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    auto expected = test::golden::Image{16, 16, std::vector<std::uint8_t>(16U * 16U * 3U, 0U)};
    const auto fill = [&expected](test::golden::Region region,
                                  std::array<std::uint8_t, 3> rgb) {
        for (int y = region.y; y < region.y + region.h; ++y) {
            for (int x = region.x; x < region.x + region.w; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * 16U +
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
    const auto actual = test::golden::from_renderer(renderer);
    const auto result = test::golden::compare(expected, actual, options);
    NUI_CHECK(result.matched);
    NUI_CHECK(result.compared_pixels == 36U);
}

class TestResourceProvider final : public ui::ResourceProvider {
public:
    std::optional<std::vector<std::byte>> load(std::string_view resource_id) override {
        ++load_count;
        if (resource_id == "tiny") {
            return std::vector<std::byte>{kTinyRgbaPng.begin(), kTinyRgbaPng.end()};
        }
        if (resource_id == "broken") {
            return std::vector<std::byte>{std::byte{0x00}, std::byte{0x01}};
        }
        return std::nullopt;
    }

    int load_count{};
};

void cache_and_errors() {
    TestResourceProvider provider;
    ui::ImageCache cache{provider};

    const auto first = cache.load("tiny");
    NUI_CHECK(first);
    NUI_CHECK(first.error == ui::ImageLoadError::None);
    NUI_CHECK(first.image.valid());
    NUI_CHECK(provider.load_count == 1);
    NUI_CHECK(cache.size() == 1U);

    const auto second = cache.load("tiny");
    NUI_CHECK(second);
    NUI_CHECK(second.image == first.image);
    NUI_CHECK(provider.load_count == 1);

    const auto missing = cache.load("missing");
    NUI_CHECK(!missing);
    NUI_CHECK(missing.error == ui::ImageLoadError::NotFound);
    NUI_CHECK(provider.load_count == 2);
    const auto missing_again = cache.load("missing");
    NUI_CHECK(!missing_again);
    NUI_CHECK(provider.load_count == 2);

    const auto broken = cache.load("broken");
    NUI_CHECK(!broken);
    NUI_CHECK(broken.error == ui::ImageLoadError::DecodeFailed);
    NUI_CHECK(provider.load_count == 3);
    NUI_CHECK(cache.size() == 3U);

    cache.clear();
    NUI_CHECK(cache.size() == 0U);
}

void invalid_image_is_explicit_and_safe() {
    const std::array<std::byte, 0> empty{};
    const auto image = ui::Image::decode(empty);
    NUI_CHECK(!image.valid());
    NUI_CHECK(image.size().w == 0.0f && image.size().h == 0.0f);

    ui::UI tree{ui::Canvas{4.0f, 4.0f, [image](ui::CanvasContext2D& canvas) {
        canvas.draw_image(image, {0.0f, 0.0f, 4.0f, 4.0f});
    }}};
    ui::HeadlessRenderer renderer{{4.0f, 4.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
}

void suite() {
    decode_and_draw();
    source_rect_and_fit_modes();
    scaling_golden();
    cache_and_errors();
    invalid_image_is_explicit_and_safe();
}

} // namespace

int main() { return test::run("image", &suite); }
