#include "test_support.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
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

constexpr std::array<std::byte, 101> kMipIsolationPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{16}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{16},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{31},
    std::byte{243}, std::byte{255}, std::byte{97}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{44}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{100}, std::byte{248}, std::byte{207}, std::byte{240},
    std::byte{159}, std::byte{129}, std::byte{2}, std::byte{192}, std::byte{196}, std::byte{64},
    std::byte{33}, std::byte{24}, std::byte{120}, std::byte{3}, std::byte{88}, std::byte{208},
    std::byte{5}, std::byte{254}, std::byte{51}, std::byte{226}, std::byte{215}, std::byte{192},
    std::byte{248}, std::byte{127}, std::byte{216}, std::byte{133}, std::byte{193}, std::byte{168},
    std::byte{1}, std::byte{12}, std::byte{12}, std::byte{140}, std::byte{163}, std::byte{121},
    std::byte{129}, std::byte{1}, std::byte{0}, std::byte{49}, std::byte{248}, std::byte{6},
    std::byte{29}, std::byte{65}, std::byte{44}, std::byte{30}, std::byte{139}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78},
    std::byte{68}, std::byte{174}, std::byte{66}, std::byte{96}, std::byte{130},
};

constexpr std::array<std::byte, 106> kLodPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{16}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{16},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{31},
    std::byte{243}, std::byte{255}, std::byte{97}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{49}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{252}, std::byte{255}, std::byte{255}, std::byte{255},
    std::byte{127}, std::byte{6}, std::byte{52}, std::byte{192}, std::byte{200}, std::byte{200},
    std::byte{8}, std::byte{103}, std::byte{99}, std::byte{145}, std::byte{70}, std::byte{1},
    std::byte{76}, std::byte{12}, std::byte{20}, std::byte{2}, std::byte{138}, std::byte{13},
    std::byte{96}, std::byte{33}, std::byte{164}, std::byte{0}, std::byte{221}, std::byte{59},
    std::byte{200}, std::byte{252}, std::byte{193}, std::byte{225}, std::byte{133}, std::byte{81},
    std::byte{3}, std::byte{70}, std::byte{13}, std::byte{24}, std::byte{28}, std::byte{6},
    std::byte{0}, std::byte{0}, std::byte{164}, std::byte{56}, std::byte{10}, std::byte{33},
    std::byte{156}, std::byte{11}, std::byte{106}, std::byte{209}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68},
    std::byte{174}, std::byte{66}, std::byte{96}, std::byte{130},
};

static_assert(noexcept(std::declval<ui::ImageTexture&>().set_transform(
    ui::Transform2D::identity())));
static_assert(noexcept(std::declval<const ui::ImageTexture&>().transform()));
static_assert(std::is_same_v<
    decltype(std::declval<ui::ImageTexture&>().set_transform(
        ui::Transform2D::identity())),
    ui::ImageTexture&>);

[[nodiscard]] bool same_bits(float a, float b) noexcept {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] bool same_transform(const ui::Transform2D& a,
                                  const ui::Transform2D& b) noexcept {
    return same_bits(a.m00, b.m00) &&
           same_bits(a.m01, b.m01) &&
           same_bits(a.m02, b.m02) &&
           same_bits(a.m10, b.m10) &&
           same_bits(a.m11, b.m11) &&
           same_bits(a.m12, b.m12);
}

[[nodiscard]] bool red(ui::Rgba8 p) noexcept {
    return p.r > 180 && p.g < 70 && p.b < 70;
}

[[nodiscard]] bool green(ui::Rgba8 p) noexcept {
    return p.g > 180 && p.r < 70 && p.b < 70;
}

[[nodiscard]] bool blue(ui::Rgba8 p) noexcept {
    return p.b > 180 && p.r < 70 && p.g < 70;
}

[[nodiscard]] bool white(ui::Rgba8 p) noexcept {
    return p.r > 220 && p.g > 220 && p.b > 220;
}

[[nodiscard]] bool black(ui::Rgba8 p) noexcept {
    return p.r < 10 && p.g < 10 && p.b < 10;
}

struct Rendered {
    int width{};
    int height{};
    std::vector<ui::Rgba8> pixels;

    [[nodiscard]] ui::Rgba8 at(int x, int y) const {
        NUI_CHECK(x >= 0 && y >= 0 && x < width && y < height);
        return pixels[
            static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x)];
    }
};

[[nodiscard]] Rendered render_brush(const ui::Brush& brush,
                                    int width,
                                    int height) {
    ui::UI tree{ui::Canvas{
        static_cast<float>(width), static_cast<float>(height),
        [brush, width, height](ui::CanvasContext2D& g) {
            const ui::Rect bounds{
                0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
            g.fill_rect(bounds, {0.0f, 0.0f, 0.0f, 1.0f});
            g.fill_rect(bounds, brush);
        }}};
    ui::HeadlessRenderer renderer{
        {static_cast<float>(width), static_cast<float>(height)}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    Rendered result;
    result.width = width;
    result.height = height;
    result.pixels.reserve(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) result.pixels.push_back(renderer.pixel(x, y));
    }
    return result;
}

[[nodiscard]] Rendered render_texture(const ui::ImageTexture& texture,
                                      int width,
                                      int height) {
    return render_brush(ui::Brush{texture}, width, height);
}

[[nodiscard]] bool pixels_differ(const Rendered& a, const Rendered& b) noexcept {
    if (a.pixels.size() != b.pixels.size()) return true;
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        const auto x = a.pixels[i];
        const auto y = b.pixels[i];
        if (x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a) return true;
    }
    return false;
}

[[nodiscard]] ui::TextureSampling nearest_no_mip() noexcept {
    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Nearest)
            .set_mipmap(ui::TextureMipmap::None);
    return sampling;
}

void api_value_and_validity_contract() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {8.0f, 6.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Nearest)
            .set_mipmap(ui::TextureMipmap::Linear);
    texture.set_sampling(sampling);

    NUI_CHECK(texture.valid());
    NUI_CHECK(same_transform(texture.transform(), ui::Transform2D::identity()));

    const auto source = texture.source();
    const auto destination = texture.destination();
    const auto transform =
        ui::Transform2D::translation(5.0f, -3.0f) *
        ui::Transform2D{1.0f, 0.25f, 0.0f, -0.1f, 1.0f, 0.0f};
    auto* returned = &texture.set_transform(transform);
    NUI_CHECK(returned == &texture);
    NUI_CHECK(texture.valid());
    NUI_CHECK(same_transform(texture.transform(), transform));
    NUI_CHECK(texture.image() == image);
    NUI_CHECK(texture.source().x == source.x && texture.source().y == source.y &&
              texture.source().w == source.w && texture.source().h == source.h);
    NUI_CHECK(texture.destination().x == destination.x &&
              texture.destination().y == destination.y &&
              texture.destination().w == destination.w &&
              texture.destination().h == destination.h);
    NUI_CHECK(texture.tile_mode_x() == ui::TextureTileMode::Repeat);
    NUI_CHECK(texture.tile_mode_y() == ui::TextureTileMode::Mirror);
    NUI_CHECK(texture.sampling().filter() == ui::TextureFilter::Nearest);
    NUI_CHECK(texture.sampling().mipmap() == ui::TextureMipmap::Linear);

    const std::array<ui::Transform2D, 8> parity{
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 0.9e-8f, 0.0f},
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.0e-8f, 0.0f},
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, 1.1e-8f, 0.0f},
        ui::Transform2D{1.0f, 0.0f, 0.0f, 0.0f, -1.1e-8f, 0.0f},
        ui::Transform2D{1.0e-20f, 0.0f, 0.0f, 0.0f, 2.0e-20f, 0.0f},
        ui::Transform2D{1.0e20f, 0.0f, 0.0f, 0.0f, 2.0e20f, 0.0f},
        ui::Transform2D{1.0e-20f, 0.0f, 1.0e20f,
                        0.0f, 1.0e-20f, -1.0e20f},
        ui::Transform2D::identity(),
    };
    for (const auto& candidate : parity) {
        texture.set_transform(candidate);
        NUI_CHECK(texture.valid() == candidate.inverse().has_value());
        NUI_CHECK(same_transform(texture.transform(), candidate));
    }

    const float payload_nan = std::bit_cast<float>(std::uint32_t{0x7fc12345U});
    const ui::Transform2D raw_nan{
        1.0f, payload_nan, 2.0f,
        0.0f, 1.0f, -4.0f};
    texture.set_transform(raw_nan);
    NUI_CHECK(!texture.valid());
    NUI_CHECK(same_transform(texture.transform(), raw_nan));
    NUI_CHECK(texture.image() == image);
    NUI_CHECK(texture.tile_mode_x() == ui::TextureTileMode::Repeat);
    NUI_CHECK(texture.sampling().mipmap() == ui::TextureMipmap::Linear);

    const ui::Transform2D negative_inf{
        1.0f, 0.0f, -std::numeric_limits<float>::infinity(),
        0.0f, 1.0f, 0.0f};
    texture.set_transform(negative_inf);
    NUI_CHECK(!texture.valid());
    NUI_CHECK(same_transform(texture.transform(), negative_inf));

    texture.set_transform(ui::Transform2D::identity());
    NUI_CHECK(texture.valid());

    ui::ImageTexture copied{texture};
    copied.set_transform(transform);
    NUI_CHECK(same_transform(copied.transform(), transform));
    NUI_CHECK(same_transform(texture.transform(), ui::Transform2D::identity()));

    ui::ImageTexture copy_assigned;
    copy_assigned = copied;
    NUI_CHECK(copy_assigned.valid());
    NUI_CHECK(same_transform(copy_assigned.transform(), transform));

    ui::ImageTexture move_assigned;
    move_assigned = std::move(copy_assigned);
    NUI_CHECK(move_assigned.valid());
    NUI_CHECK(same_transform(move_assigned.transform(), transform));
    NUI_CHECK(!copy_assigned.valid());
    NUI_CHECK(same_transform(
        copy_assigned.transform(), ui::Transform2D::identity()));

    ui::ImageTexture moved{std::move(copied)};
    NUI_CHECK(moved.valid());
    NUI_CHECK(same_transform(moved.transform(), transform));
    NUI_CHECK(!copied.valid());
    NUI_CHECK(same_transform(copied.transform(), ui::Transform2D::identity()));

    auto* alias = &moved;
    moved = std::move(*alias);
    NUI_CHECK(!moved.valid());
    NUI_CHECK(same_transform(moved.transform(), ui::Transform2D::identity()));

    ui::ImageTexture invalid_base;
    invalid_base.set_transform(transform);
    NUI_CHECK(!invalid_base.valid());
    NUI_CHECK(same_transform(invalid_base.transform(), transform));

    ui::ImageTexture first{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    ui::ImageTexture second = first;
    first.set_transform(ui::Transform2D::translation(3.0f, 4.0f));
    second.set_transform(ui::Transform2D::scaling(2.0f, 3.0f));
    NUI_CHECK(first.image() == second.image());
    NUI_CHECK(first.transform().m02 == 3.0f && first.transform().m12 == 4.0f);
    NUI_CHECK(second.transform().m00 == 2.0f && second.transform().m11 == 3.0f);
}

void transform_golden(const ui::Image& image,
                      const ui::Transform2D& transform) {
    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest_no_mip());
    texture.set_transform(transform);
    NUI_CHECK(texture.valid());

    ui::UI tree{ui::Canvas{64.0f, 64.0f, [brush = ui::Brush{texture}](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, brush);
    }}};
    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    struct Probe {
        ui::Point base;
        std::array<std::uint8_t, 3> rgb;
    };
    constexpr std::array<Probe, 4> probes{{
        {{4.0f, 4.0f}, {255, 0, 0}},
        {{12.0f, 4.0f}, {0, 255, 0}},
        {{4.0f, 12.0f}, {0, 0, 255}},
        {{12.0f, 12.0f}, {255, 255, 255}},
    }};

    test::golden::Image expected{
        64, 64, std::vector<std::uint8_t>(64U * 64U * 3U, 0U)};
    test::golden::CompareOptions options;
    options.channel_tolerance = 4;

    for (const auto& probe : probes) {
        const auto mapped = transform.map_point(probe.base);
        const int x = static_cast<int>(std::lround(mapped.x));
        const int y = static_cast<int>(std::lround(mapped.y));
        NUI_CHECK(x >= 0 && y >= 0 && x < 64 && y < 64);
        const auto offset =
            (static_cast<std::size_t>(y) * 64U + static_cast<std::size_t>(x)) * 3U;
        expected.rgb[offset + 0] = probe.rgb[0];
        expected.rgb[offset + 1] = probe.rgb[1];
        expected.rgb[offset + 2] = probe.rgb[2];
        options.compare_regions.push_back({x, y, 1, 1});
    }
    options.compare_regions.push_back({60, 60, 1, 1});

    const auto result =
        test::golden::compare(expected, test::golden::from_renderer(renderer), options);
    NUI_CHECK(result.matched);
    NUI_CHECK(result.compared_pixels == 5U);
}

void transform_render_goldens() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    transform_golden(image, ui::Transform2D::identity());
    transform_golden(image, ui::Transform2D::translation(16.0f, 8.0f));
    transform_golden(image, ui::Transform2D::scaling(2.0f, 1.0f));
    transform_golden(
        image,
        ui::Transform2D::translation(32.0f, 0.0f) *
            ui::Transform2D::rotation(ui::kPi * 0.5f));
    transform_golden(
        image,
        ui::Transform2D::translation(8.0f, 8.0f) *
            ui::Transform2D{1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f});
}

void nonzero_destination_origin_uses_global_local_origin() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {8.0f, 8.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest_no_mip());
    const auto transform =
        ui::Transform2D::translation(40.0f, 0.0f) *
        ui::Transform2D::rotation(ui::kPi * 0.5f);
    texture.set_transform(transform);

    const auto rendered = render_texture(texture, 64, 40);
    const auto expected_red = transform.map_point({12.0f, 12.0f});
    NUI_CHECK(red(rendered.at(
        static_cast<int>(std::lround(expected_red.x)),
        static_cast<int>(std::lround(expected_red.y)))));
    NUI_CHECK(black(rendered.at(12, 12)));
}

void transformed_tile_domains_follow_texture() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto render_mode = [&](ui::TextureTileMode mode) {
        ui::ImageTexture texture{
            image, {0.0f, 0.0f, 2.0f, 2.0f}, {16.0f, 8.0f, 16.0f, 16.0f}};
        texture.set_tile_mode(mode, ui::TextureTileMode::Clamp);
        texture.set_sampling(nearest_no_mip());
        texture.set_transform(ui::Transform2D::translation(16.0f, 0.0f));
        return render_texture(texture, 64, 32);
    };

    const auto clamp = render_mode(ui::TextureTileMode::Clamp);
    NUI_CHECK(red(clamp.at(20, 12)));
    NUI_CHECK(green(clamp.at(52, 12)));

    const auto repeat = render_mode(ui::TextureTileMode::Repeat);
    NUI_CHECK(red(repeat.at(20, 12)));
    NUI_CHECK(red(repeat.at(52, 12)));

    const auto mirror = render_mode(ui::TextureTileMode::Mirror);
    NUI_CHECK(green(mirror.at(20, 12)));
    NUI_CHECK(green(mirror.at(52, 12)));

    const auto decal = render_mode(ui::TextureTileMode::Decal);
    NUI_CHECK(black(decal.at(20, 12)));
    NUI_CHECK(black(decal.at(52, 12)));
    NUI_CHECK(red(decal.at(36, 12)));
}

void invalid_transform_is_transparent_and_recovers() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest_no_mip());

    const auto image_before = texture.image();
    const auto source_before = texture.source();
    const auto destination_before = texture.destination();
    const auto sampling_before = texture.sampling();

    const float inf = std::numeric_limits<float>::infinity();
    const ui::Transform2D invalid{
        1.0f, 0.0f, inf,
        0.0f, 1.0f, 0.0f};
    texture.set_transform(invalid);
    NUI_CHECK(!texture.valid());
    NUI_CHECK(same_transform(texture.transform(), invalid));
    NUI_CHECK(texture.image() == image_before);
    NUI_CHECK(texture.source().x == source_before.x &&
              texture.source().y == source_before.y &&
              texture.source().w == source_before.w &&
              texture.source().h == source_before.h);
    NUI_CHECK(texture.destination().x == destination_before.x &&
              texture.destination().y == destination_before.y &&
              texture.destination().w == destination_before.w &&
              texture.destination().h == destination_before.h);
    NUI_CHECK(texture.sampling().filter() == sampling_before.filter());
    NUI_CHECK(texture.sampling().mipmap() == sampling_before.mipmap());

    const auto transparent = render_texture(texture, 32, 32);
    NUI_CHECK(black(transparent.at(4, 4)));

    texture.set_transform(ui::Transform2D::identity());
    NUI_CHECK(texture.valid());
    const auto recovered = render_texture(texture, 32, 32);
    NUI_CHECK(red(recovered.at(4, 4)));
}

void brush_snapshot_preserves_valid_transform() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest_no_mip());
    texture.set_transform(ui::Transform2D::translation(8.0f, 8.0f));
    const ui::Brush old_brush{texture};

    texture.set_transform(ui::Transform2D::translation(32.0f, 8.0f));
    const ui::Brush new_brush{texture};

    const auto old_render = render_brush(old_brush, 64, 32);
    const auto new_render = render_brush(new_brush, 64, 32);
    NUI_CHECK(red(old_render.at(12, 12)));
    NUI_CHECK(black(old_render.at(36, 12)));
    NUI_CHECK(black(new_render.at(12, 12)));
    NUI_CHECK(red(new_render.at(36, 12)));

    const auto invalid = ui::Transform2D{
        1.0f, 0.0f, std::numeric_limits<float>::infinity(),
        0.0f, 1.0f, 0.0f};
    texture.set_transform(invalid);
    const ui::Brush transparent_brush{texture};
    NUI_CHECK(same_transform(texture.transform(), invalid));
    NUI_CHECK(black(render_brush(transparent_brush, 64, 32).at(36, 12)));
}

void painter_and_texture_noncommuting_order() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest_no_mip());
    const ui::Transform2D texture_transform{
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f};
    texture.set_transform(texture_transform);

    const auto info = SkImageInfo::Make(
        144, 128, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(surface != nullptr);
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    canvas->clear(SK_ColorBLACK);

    ui::Painter painter{*canvas};
    painter.translate(40.0f, 60.0f);
    const auto painter_transform = painter.current_transform();
    painter.fill_rounded_rect(
        {-100.0f, -100.0f, 200.0f, 200.0f}, 0.0f, ui::Brush{texture});

    const ui::Point pattern_point{4.0f, 4.0f};
    const auto expected =
        (painter_transform * texture_transform).map_point(pattern_point);
    const auto opposite =
        (texture_transform * painter_transform).map_point(pattern_point);
    NUI_CHECK(std::fabs(expected.x - opposite.x) > 20.0f);

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap));
    const auto expected_pixel = pixmap.getColor(
        static_cast<int>(std::lround(expected.x)),
        static_cast<int>(std::lround(expected.y)));
    const auto opposite_pixel = pixmap.getColor(
        static_cast<int>(std::lround(opposite.x)),
        static_cast<int>(std::lround(opposite.y)));
    NUI_CHECK(SkColorGetR(expected_pixel) > 180);
    NUI_CHECK(SkColorGetG(expected_pixel) < 70);
    NUI_CHECK(SkColorGetB(expected_pixel) < 70);
    NUI_CHECK(SkColorGetR(opposite_pixel) < 10);
    NUI_CHECK(SkColorGetG(opposite_pixel) < 10);
    NUI_CHECK(SkColorGetB(opposite_pixel) < 10);
}

void transformed_sampling_participates_in_lod() {
    const auto image = ui::Image::decode(kLodPng);
    NUI_CHECK(image.valid());

    const auto make = [&](ui::TextureMipmap mipmap,
                          ui::Transform2D transform) {
        ui::ImageTexture texture{
            image, {0.0f, 0.0f, 16.0f, 16.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
        texture.set_tile_mode(
            ui::TextureTileMode::Repeat, ui::TextureTileMode::Repeat);
        ui::TextureSampling sampling;
        sampling.set_filter(ui::TextureFilter::Linear).set_mipmap(mipmap);
        texture.set_sampling(sampling);
        texture.set_transform(transform);
        return render_texture(texture, 24, 24);
    };

    const auto identity_none =
        make(ui::TextureMipmap::None, ui::Transform2D::identity());
    const auto identity_linear =
        make(ui::TextureMipmap::Linear, ui::Transform2D::identity());
    NUI_CHECK(!pixels_differ(identity_none, identity_linear));

    const auto shrink = ui::Transform2D::scaling(0.375f, 0.375f);
    const auto none = make(ui::TextureMipmap::None, shrink);
    const auto nearest = make(ui::TextureMipmap::Nearest, shrink);
    const auto linear = make(ui::TextureMipmap::Linear, shrink);
    NUI_CHECK(pixels_differ(none, nearest));
    NUI_CHECK(pixels_differ(nearest, linear));
}

void transformed_fractional_source_mips_stay_isolated() {
    const auto image = ui::Image::decode(kMipIsolationPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image,
        {4.25f, 4.25f, 7.5f, 7.5f},
        {8.0f, 8.0f, 15.0f, 15.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Repeat);
    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Linear)
            .set_mipmap(ui::TextureMipmap::Linear);
    texture.set_sampling(sampling);
    texture.set_transform(ui::Transform2D::scaling(0.2f, 0.2f));
    NUI_CHECK(texture.valid());

    const auto rendered = render_texture(texture, 32, 32);
    constexpr std::array<int, 4> probes{2, 9, 17, 26};
    for (const int y : probes) {
        for (const int x : probes) NUI_CHECK(red(rendered.at(x, y)));
    }
}

void suite() {
    api_value_and_validity_contract();
    transform_render_goldens();
    nonzero_destination_origin_uses_global_local_origin();
    transformed_tile_domains_follow_texture();
    invalid_transform_is_transparent_and_recovers();
    brush_snapshot_preserves_valid_transform();
    painter_and_texture_noncommuting_order();
    transformed_sampling_participates_in_lod();
    transformed_fractional_source_mips_stay_isolated();
}

} // namespace

int main() {
    return test::run("T086 ImageTexture affine transforms", &suite);
}
