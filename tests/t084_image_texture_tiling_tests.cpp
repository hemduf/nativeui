#include "test_support.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

[[nodiscard]] bool red(ui::Rgba8 p) noexcept {
    return p.r > 180 && p.g < 70 && p.b < 70;
}

[[nodiscard]] bool green(ui::Rgba8 p) noexcept {
    return p.g > 180 && p.r < 70 && p.b < 70;
}

[[nodiscard]] bool blue(ui::Rgba8 p) noexcept {
    return p.b > 180 && p.r < 70 && p.g < 70;
}

[[nodiscard]] bool black(ui::Rgba8 p) noexcept {
    return p.r < 8 && p.g < 8 && p.b < 8;
}

[[nodiscard]] bool near(float a, float b) noexcept {
    return std::fabs(a - b) <= 1.0e-6f;
}

struct AxisReference {
    bool visible{true};
    float q{};
};

[[nodiscard]] AxisReference reference_coordinate(
    ui::TextureTileMode mode,
    float q) noexcept {
    switch (mode) {
        case ui::TextureTileMode::Clamp:
            return {true, q < 0.0f ? 0.0f : (q > 1.0f ? 1.0f : q)};
        case ui::TextureTileMode::Repeat: {
            const float n = std::floor(q);
            return {true, q - n};
        }
        case ui::TextureTileMode::Mirror: {
            const float n = std::floor(q);
            const float f = q - n;
            const float period = n - 2.0f * std::floor(n * 0.5f);
            return {true, period == 0.0f ? f : 1.0f - f};
        }
        case ui::TextureTileMode::Decal:
            return q < 0.0f || q > 1.0f
                ? AxisReference{false, 0.0f}
                : AxisReference{true, q};
    }
    return {false, 0.0f};
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

[[nodiscard]] Rendered render_texture(
    const ui::ImageTexture& texture,
    int width,
    int height) {
    const ui::Brush brush{texture};
    ui::UI tree{ui::Canvas{
        static_cast<float>(width),
        static_cast<float>(height),
        [brush, width, height](ui::CanvasContext2D& g) {
            const ui::Rect bounds{
                0.0f, 0.0f,
                static_cast<float>(width),
                static_cast<float>(height)};
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
        for (int x = 0; x < width; ++x) {
            result.pixels.push_back(renderer.pixel(x, y));
        }
    }
    return result;
}

void api_and_reference_contract() {
    static_assert(noexcept(std::declval<ui::ImageTexture&>().set_tile_mode(
        ui::TextureTileMode::Clamp, ui::TextureTileMode::Clamp)));
    static_assert(noexcept(std::declval<const ui::ImageTexture&>().tile_mode_x()));
    static_assert(noexcept(std::declval<const ui::ImageTexture&>().tile_mode_y()));
    static_assert(std::is_same_v<
        decltype(std::declval<ui::ImageTexture&>().set_tile_mode(
            ui::TextureTileMode::Clamp, ui::TextureTileMode::Clamp)),
        ui::ImageTexture&>);

    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {4.0f, 5.0f, 16.0f, 12.0f}};
    NUI_CHECK(texture.tile_mode_x() == ui::TextureTileMode::Clamp);
    NUI_CHECK(texture.tile_mode_y() == ui::TextureTileMode::Clamp);

    const auto source = texture.source();
    const auto destination = texture.destination();
    auto* returned = &texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    NUI_CHECK(returned == &texture);
    NUI_CHECK(texture.tile_mode_x() == ui::TextureTileMode::Repeat);
    NUI_CHECK(texture.tile_mode_y() == ui::TextureTileMode::Mirror);
    NUI_CHECK(texture.image() == image);
    NUI_CHECK(texture.source().x == source.x && texture.source().y == source.y &&
              texture.source().w == source.w && texture.source().h == source.h);
    NUI_CHECK(texture.destination().x == destination.x &&
              texture.destination().y == destination.y &&
              texture.destination().w == destination.w &&
              texture.destination().h == destination.h);

    auto copied = texture;
    copied.set_tile_mode(ui::TextureTileMode::Decal, ui::TextureTileMode::Clamp);
    NUI_CHECK(texture.tile_mode_x() == ui::TextureTileMode::Repeat);
    NUI_CHECK(texture.tile_mode_y() == ui::TextureTileMode::Mirror);
    NUI_CHECK(copied.tile_mode_x() == ui::TextureTileMode::Decal);
    NUI_CHECK(copied.tile_mode_y() == ui::TextureTileMode::Clamp);
    NUI_CHECK(copied.image() == texture.image());

    ui::ImageTexture moved{std::move(copied)};
    NUI_CHECK(moved.tile_mode_x() == ui::TextureTileMode::Decal);
    NUI_CHECK(moved.tile_mode_y() == ui::TextureTileMode::Clamp);
    NUI_CHECK(copied.tile_mode_x() == ui::TextureTileMode::Clamp);
    NUI_CHECK(copied.tile_mode_y() == ui::TextureTileMode::Clamp);

    auto* moved_alias = &moved;
    moved = std::move(*moved_alias);
    NUI_CHECK(!moved.valid());
    NUI_CHECK(moved.tile_mode_x() == ui::TextureTileMode::Clamp);
    NUI_CHECK(moved.tile_mode_y() == ui::TextureTileMode::Clamp);

    struct ReferenceCase {
        float q;
        float clamp;
        float repeat;
        float mirror;
        bool decal_visible;
        float decal;
    };
    constexpr std::array<ReferenceCase, 8> cases{{
        {-2.25f, 0.0f, 0.75f, 0.25f, false, 0.0f},
        {-1.25f, 0.0f, 0.75f, 0.75f, false, 0.0f},
        {-0.25f, 0.0f, 0.75f, 0.25f, false, 0.0f},
        {0.0f,   0.0f, 0.0f,  0.0f,  true,  0.0f},
        {0.25f,  0.25f,0.25f, 0.25f, true,  0.25f},
        {1.0f,   1.0f, 0.0f,  1.0f,  true,  1.0f},
        {1.25f,  1.0f, 0.25f, 0.75f, false, 0.0f},
        {2.25f,  1.0f, 0.25f, 0.25f, false, 0.0f},
    }};

    for (const auto& item : cases) {
        const auto clamp =
            reference_coordinate(ui::TextureTileMode::Clamp, item.q);
        const auto repeat =
            reference_coordinate(ui::TextureTileMode::Repeat, item.q);
        const auto mirror =
            reference_coordinate(ui::TextureTileMode::Mirror, item.q);
        const auto decal =
            reference_coordinate(ui::TextureTileMode::Decal, item.q);
        NUI_CHECK(clamp.visible && near(clamp.q, item.clamp));
        NUI_CHECK(repeat.visible && near(repeat.q, item.repeat));
        NUI_CHECK(mirror.visible && near(mirror.q, item.mirror));
        NUI_CHECK(decal.visible == item.decal_visible);
        if (decal.visible) NUI_CHECK(near(decal.q, item.decal));
    }
}

void default_and_explicit_clamp_are_identical() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture implicit{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {4.0f, 4.0f, 16.0f, 16.0f}};
    auto explicit_clamp = implicit;
    explicit_clamp.set_tile_mode(
        ui::TextureTileMode::Clamp, ui::TextureTileMode::Clamp);

    const auto a = render_texture(implicit, 24, 24);
    const auto b = render_texture(explicit_clamp, 24, 24);
    NUI_CHECK(a.pixels.size() == b.pixels.size());
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        NUI_CHECK(a.pixels[i].r == b.pixels[i].r);
        NUI_CHECK(a.pixels[i].g == b.pixels[i].g);
        NUI_CHECK(a.pixels[i].b == b.pixels[i].b);
        NUI_CHECK(a.pixels[i].a == b.pixels[i].a);
    }
}

void negative_coordinate_rendering_matches_reference() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    const auto make = [&](ui::TextureTileMode x) {
        ui::ImageTexture texture{
            image, {0.0f, 0.0f, 2.0f, 2.0f}, {32.0f, 8.0f, 16.0f, 16.0f}};
        texture.set_tile_mode(x, ui::TextureTileMode::Clamp);
        return render_texture(texture, 80, 32);
    };

    // These probes map to texel centers after normalization, so Linear and
    // Nearest produce the same deterministic color and avoid half-texel
    // boundary ambiguity.
    const auto clamp = make(ui::TextureTileMode::Clamp);
    NUI_CHECK(red(clamp.at(12, 12)));   // q=-1.25 -> 0
    NUI_CHECK(red(clamp.at(28, 12)));   // q=-0.25 -> 0
    NUI_CHECK(green(clamp.at(52, 12))); // q=1.25 -> 1
    NUI_CHECK(green(clamp.at(68, 12))); // q=2.25 -> 1

    const auto repeat = make(ui::TextureTileMode::Repeat);
    NUI_CHECK(green(repeat.at(12, 12))); // -1.25 -> .75
    NUI_CHECK(green(repeat.at(28, 12))); // -.25 -> .75
    NUI_CHECK(red(repeat.at(52, 12)));   // 1.25 -> .25
    NUI_CHECK(red(repeat.at(68, 12)));   // 2.25 -> .25

    const auto mirror = make(ui::TextureTileMode::Mirror);
    NUI_CHECK(green(mirror.at(12, 12))); // -1.25 -> .75
    NUI_CHECK(red(mirror.at(28, 12)));   // -.25 -> .25
    NUI_CHECK(green(mirror.at(52, 12))); // 1.25 -> .75
    NUI_CHECK(red(mirror.at(68, 12)));   // 2.25 -> .25

    const auto decal = make(ui::TextureTileMode::Decal);
    NUI_CHECK(black(decal.at(12, 12)));
    NUI_CHECK(black(decal.at(28, 12)));
    NUI_CHECK(red(decal.at(36, 12)));    // .25 stays inside
    NUI_CHECK(black(decal.at(52, 12)));
    NUI_CHECK(black(decal.at(68, 12)));
}

void mixed_axes_are_independent() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {16.0f, 16.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    const auto rendered = render_texture(texture, 48, 48);

    // (1.25, 1.25) -> (.25, .75): bottom-left texel.
    NUI_CHECK(blue(rendered.at(36, 36)));
    // (-.25, -.25) -> (.75, .25): top-right texel.
    NUI_CHECK(green(rendered.at(12, 12)));
}

void selected_source_isolation_for_every_mode() {
    const auto image = ui::Image::decode(kCropIsolationPng);
    NUI_CHECK(image.valid());

    constexpr std::array<ui::TextureTileMode, 3> opaque_modes{
        ui::TextureTileMode::Clamp,
        ui::TextureTileMode::Repeat,
        ui::TextureTileMode::Mirror,
    };
    constexpr std::array<int, 5> xs{12, 28, 36, 52, 68};
    constexpr std::array<int, 5> ys{4, 20, 28, 44, 60};

    const auto verify_opaque = [&](ui::Rect source, ui::TextureTileMode mode) {
        ui::ImageTexture texture{
            image, source, {32.0f, 24.0f, 16.0f, 16.0f}};
        texture.set_tile_mode(mode, mode);
        const auto rendered = render_texture(texture, 80, 72);
        for (const int y : ys) {
            for (const int x : xs) {
                NUI_CHECK(red(rendered.at(x, y)));
            }
        }
    };

    for (const auto mode : opaque_modes) {
        verify_opaque({1.0f, 0.0f, 2.0f, 2.0f}, mode);
        // Fractional T083 source rectangles must keep the same isolation when
        // repeated/mirrored/clamped across multiple periods.
        verify_opaque({1.25f, 0.0f, 1.5f, 2.0f}, mode);
    }

    const auto verify_decal = [&](ui::Rect source) {
        ui::ImageTexture decal{
            image, source, {32.0f, 24.0f, 16.0f, 16.0f}};
        decal.set_tile_mode(
            ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
        const auto rendered = render_texture(decal, 80, 72);
        NUI_CHECK(red(rendered.at(36, 28)));
        NUI_CHECK(black(rendered.at(28, 28)));
        NUI_CHECK(black(rendered.at(52, 28)));
        NUI_CHECK(black(rendered.at(36, 20)));
        NUI_CHECK(black(rendered.at(36, 44)));
    };
    verify_decal({1.0f, 0.0f, 2.0f, 2.0f});
    verify_decal({1.25f, 0.0f, 1.5f, 2.0f});
}

void painter_transform_preserves_pattern_space_tiling() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    const ui::Brush brush{texture};

    ui::UI tree{ui::Canvas{64.0f, 16.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, brush);
        g.save();
        g.translate(32.0f, 0.0f);
        g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, brush);
        g.restore();
    }}};
    ui::HeadlessRenderer renderer{{64.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 32; ++x) {
            const auto a = renderer.pixel(x, y);
            const auto b = renderer.pixel(x + 32, y);
            NUI_CHECK(a.r == b.r && a.g == b.g &&
                      a.b == b.b && a.a == b.a);
        }
    }
}

void shared_image_texture_state_is_independent() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture first{image, {0.0f, 0.0f, 16.0f, 16.0f}};
    ui::ImageTexture second{image, {0.0f, 0.0f, 16.0f, 16.0f}};
    first.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    second.set_tile_mode(
        ui::TextureTileMode::Decal, ui::TextureTileMode::Clamp);

    NUI_CHECK(first.image() == second.image());
    NUI_CHECK(first.tile_mode_x() == ui::TextureTileMode::Repeat);
    NUI_CHECK(first.tile_mode_y() == ui::TextureTileMode::Mirror);
    NUI_CHECK(second.tile_mode_x() == ui::TextureTileMode::Decal);
    NUI_CHECK(second.tile_mode_y() == ui::TextureTileMode::Clamp);

    first.set_tile_mode(
        ui::TextureTileMode::Clamp, ui::TextureTileMode::Repeat);
    NUI_CHECK(second.tile_mode_x() == ui::TextureTileMode::Decal);
    NUI_CHECK(second.tile_mode_y() == ui::TextureTileMode::Clamp);
    NUI_CHECK(second.image() == image);
}

void shader_child_preserves_tiling() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    NUI_CHECK(image.valid());

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Clamp);

    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader source;
        half4 main(float2 p) {
            return source.eval(p);
        }
    )");
    NUI_CHECK(compiled.ok());

    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.set_child("source", ui::Brush{texture}) ==
              ui::ShaderSetResult::Ok);
    const ui::Brush shader{instance};

    ui::UI tree{ui::Canvas{32.0f, 16.0f, [shader](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, shader);
    }}};
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red(renderer.pixel(4, 4)));
    NUI_CHECK(green(renderer.pixel(12, 4)));
    NUI_CHECK(red(renderer.pixel(20, 4)));
    NUI_CHECK(green(renderer.pixel(28, 4)));
}

void suite() {
    api_and_reference_contract();
    default_and_explicit_clamp_are_identical();
    negative_coordinate_rendering_matches_reference();
    mixed_axes_are_independent();
    selected_source_isolation_for_every_mode();
    painter_transform_preserves_pattern_space_tiling();
    shared_image_texture_state_is_independent();
    shader_child_preserves_tiling();
}

} // namespace

int main() {
    return test::run("T084 ImageTexture tiling", &suite);
}
