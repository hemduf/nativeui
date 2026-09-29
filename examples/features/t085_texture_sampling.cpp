#include "example_support.hpp"

#include <nativeui/headless.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
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

// 16x16 fixture: an 8x8 solid-red selected source surrounded by solid green.
// It provides three useful mip levels for proving source-subrect isolation.
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

// 16x16 asymmetric black/white fixture whose adjacent mip levels are visibly
// different at non-integral LODs. This keeps Nearest-vs-Linear mip selection
// observable instead of collapsing to the same average at every level.
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

static_assert(noexcept(std::declval<ui::TextureSampling&>().set_filter(
    ui::TextureFilter::Nearest)));
static_assert(noexcept(std::declval<ui::TextureSampling&>().set_mipmap(
    ui::TextureMipmap::Linear)));
static_assert(noexcept(std::declval<const ui::TextureSampling&>().filter()));
static_assert(noexcept(std::declval<const ui::TextureSampling&>().mipmap()));
static_assert(noexcept(std::declval<ui::ImageTexture&>().set_sampling(
    ui::TextureSampling{})));
static_assert(noexcept(std::declval<const ui::ImageTexture&>().sampling()));
static_assert(std::is_trivially_copyable_v<ui::TextureSampling>);

[[nodiscard]] ui::TextureSampling sampling(ui::TextureFilter filter,
                                           ui::TextureMipmap mipmap) noexcept {
    ui::TextureSampling value;
    value.set_filter(filter).set_mipmap(mipmap);
    return value;
}

[[nodiscard]] bool red(ui::Rgba8 p) noexcept {
    return p.r > 180 && p.g < 70 && p.b < 70;
}

[[nodiscard]] bool green(ui::Rgba8 p) noexcept {
    return p.g > 180 && p.r < 70 && p.b < 70;
}

[[nodiscard]] bool magenta(ui::Rgba8 p) noexcept {
    return p.r > 220 && p.g < 40 && p.b > 220;
}

struct Rendered {
    int width{};
    int height{};
    std::vector<ui::Rgba8> pixels;

    [[nodiscard]] ui::Rgba8 at(int x, int y) const noexcept {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
    }
};

[[nodiscard]] Rendered render_texture(
    const ui::ImageTexture& texture,
    int width,
    int height,
    ui::Color background = {0.0f, 0.0f, 0.0f, 1.0f}) {
    const ui::Brush brush{texture};
    ui::UI tree{ui::Canvas{
        static_cast<float>(width), static_cast<float>(height),
        [brush, width, height, background](ui::CanvasContext2D& g) {
            const ui::Rect bounds{
                0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
            g.fill_rect(bounds, background);
            g.fill_rect(bounds, brush);
        }}};
    ui::HeadlessRenderer renderer{
        {static_cast<float>(width), static_cast<float>(height)}, 1.0f};
    if (!renderer.render(tree)) return {};

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

[[nodiscard]] bool pixels_differ(const Rendered& a, const Rendered& b) noexcept {
    if (a.pixels.size() != b.pixels.size()) return true;
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        const auto lhs = a.pixels[i];
        const auto rhs = b.pixels[i];
        if (lhs.r != rhs.r || lhs.g != rhs.g ||
            lhs.b != rhs.b || lhs.a != rhs.a) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::unique_ptr<ui::UI> make_demo_ui() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return {};

    ui::ImageTexture nearest_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {24.0f, 24.0f, 120.0f, 120.0f}};
    nearest_texture.set_sampling(
        sampling(ui::TextureFilter::Nearest, ui::TextureMipmap::None));

    ui::ImageTexture linear_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {168.0f, 24.0f, 120.0f, 120.0f}};
    linear_texture.set_sampling(
        sampling(ui::TextureFilter::Linear, ui::TextureMipmap::None));

    ui::ImageTexture mip_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {312.0f, 24.0f, 1.0f, 1.0f}};
    mip_texture.set_sampling(
        sampling(ui::TextureFilter::Linear, ui::TextureMipmap::Linear));
    mip_texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);

    const ui::Brush nearest{nearest_texture};
    const ui::Brush linear{linear_texture};
    const ui::Brush mip{mip_texture};

    return std::make_unique<ui::UI>(
        ui::Canvas{360.0f, 180.0f, [nearest, linear, mip](ui::CanvasContext2D& g) {
            g.fill_rect(
                {0.0f, 0.0f, 360.0f, 180.0f},
                {0.06f, 0.06f, 0.07f, 1.0f});
            g.fill_rect({24.0f, 24.0f, 120.0f, 120.0f}, nearest);
            g.fill_rect({168.0f, 24.0f, 120.0f, 120.0f}, linear);
            g.fill_rect({312.0f, 24.0f, 24.0f, 120.0f}, mip);
        }});
}

int self_test() {
    ui::TextureSampling value;
    if (value.filter() != ui::TextureFilter::Linear) {
        return example::fail("T085 default filter must be Linear");
    }
    if (value.mipmap() != ui::TextureMipmap::None) {
        return example::fail("T085 default mipmap policy must be None");
    }

    auto* returned = &value.set_filter(ui::TextureFilter::Nearest)
                         .set_mipmap(ui::TextureMipmap::Linear);
    if (returned != &value ||
        value.filter() != ui::TextureFilter::Nearest ||
        value.mipmap() != ui::TextureMipmap::Linear) {
        return example::fail("T085 TextureSampling value mutation failed");
    }

    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return example::fail("T085 sample image did not decode");

    ui::ImageTexture value_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 32.0f, 32.0f}};
    if (value_texture.sampling().filter() != ui::TextureFilter::Linear ||
        value_texture.sampling().mipmap() != ui::TextureMipmap::None) {
        return example::fail("T085 ImageTexture defaults changed T083 behavior");
    }

    // Repeated mutation must preserve a stable backend-neutral value. Heap,
    // decode and backend-materialization guards live in the dedicated fault test.
    for (int i = 0; i < 1024; ++i) {
        const auto filter = (i & 1) == 0
            ? ui::TextureFilter::Nearest
            : ui::TextureFilter::Linear;
        const auto mipmap = (i % 3) == 0
            ? ui::TextureMipmap::None
            : ((i % 3) == 1 ? ui::TextureMipmap::Nearest
                             : ui::TextureMipmap::Linear);
        value.set_filter(filter).set_mipmap(mipmap);
        value_texture.set_sampling(value);
        const auto roundtrip = value_texture.sampling();
        if (roundtrip.filter() != filter || roundtrip.mipmap() != mipmap) {
            return example::fail("T085 sampling roundtrip changed value state");
        }
    }

    // Two texture values sharing one immutable Image must keep independent
    // sampling state and leave mapping/tiling untouched.
    ui::ImageTexture first = value_texture;
    ui::ImageTexture second = value_texture;
    first.set_sampling(sampling(ui::TextureFilter::Nearest, ui::TextureMipmap::Nearest));
    second.set_sampling(sampling(ui::TextureFilter::Linear, ui::TextureMipmap::Linear));
    if (first.image() != second.image() ||
        first.sampling().filter() != ui::TextureFilter::Nearest ||
        first.sampling().mipmap() != ui::TextureMipmap::Nearest ||
        second.sampling().filter() != ui::TextureFilter::Linear ||
        second.sampling().mipmap() != ui::TextureMipmap::Linear) {
        return example::fail("T085 shared Image sampling state leaked between textures");
    }

    // Magnification: level 0 is used for every mip policy while filter choice
    // remains observable at the base-level texel transition.
    ui::ImageTexture nearest_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 32.0f, 32.0f}};
    nearest_texture.set_sampling(
        sampling(ui::TextureFilter::Nearest, ui::TextureMipmap::None));
    const auto nearest_pixels = render_texture(nearest_texture, 32, 32);
    if (nearest_pixels.pixels.empty() ||
        !red(nearest_pixels.at(4, 4)) || !green(nearest_pixels.at(27, 4))) {
        return example::fail("T085 nearest magnification samples are incorrect");
    }

    auto linear_texture = nearest_texture;
    linear_texture.set_sampling(
        sampling(ui::TextureFilter::Linear, ui::TextureMipmap::None));
    const auto linear_pixels = render_texture(linear_texture, 32, 32);
    if (linear_pixels.pixels.empty() || !pixels_differ(nearest_pixels, linear_pixels)) {
        return example::fail("T085 nearest and linear magnification are indistinguishable");
    }

    constexpr std::array<ui::TextureFilter, 2> filters{
        ui::TextureFilter::Nearest,
        ui::TextureFilter::Linear,
    };
    for (const auto filter : filters) {
        ui::ImageTexture base{
            image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 32.0f, 32.0f}};
        base.set_sampling(sampling(filter, ui::TextureMipmap::None));
        const auto no_mip = render_texture(base, 32, 32);
        base.set_sampling(sampling(filter, ui::TextureMipmap::Nearest));
        const auto nearest_mip = render_texture(base, 32, 32);
        base.set_sampling(sampling(filter, ui::TextureMipmap::Linear));
        const auto linear_mip = render_texture(base, 32, 32);
        if (no_mip.pixels.empty() || nearest_mip.pixels.empty() ||
            linear_mip.pixels.empty() || pixels_differ(no_mip, nearest_mip) ||
            pixels_differ(no_mip, linear_mip)) {
            return example::fail("T085 magnification did not stay on mip level 0");
        }
    }

    // Mipmap policy: controlled non-integral minification must exercise base
    // sampling, nearest-level selection and trilinear blending as distinct paths.
    const auto lod_image = ui::Image::decode(kLodPng);
    if (!lod_image.valid()) return example::fail("T085 LOD fixture did not decode");
    const auto make_lod = [&](ui::TextureMipmap mipmap) {
        ui::ImageTexture texture{
            lod_image, {0.0f, 0.0f, 16.0f, 16.0f}, {0.0f, 0.0f, 6.0f, 6.0f}};
        texture.set_sampling(sampling(ui::TextureFilter::Linear, mipmap));
        return render_texture(texture, 6, 6);
    };
    const auto no_mip = make_lod(ui::TextureMipmap::None);
    const auto nearest_mip = make_lod(ui::TextureMipmap::Nearest);
    const auto linear_mip = make_lod(ui::TextureMipmap::Linear);
    if (no_mip.pixels.empty() || nearest_mip.pixels.empty() || linear_mip.pixels.empty()) {
        return example::fail("T085 minification render failed");
    }
    if (!pixels_differ(no_mip, nearest_mip)) {
        return example::fail("T085 mipmap None and Nearest produced identical minification");
    }
    if (!pixels_differ(nearest_mip, linear_mip)) {
        return example::fail("T085 nearest-level and trilinear minification are indistinguishable");
    }

    // Source-subrect isolation: mipmaps must be built from the selected red 8x8
    // region rather than the green parent image. Exercise every tile mode,
    // both filters, both mip policies, and enough downscale for multiple levels.
    const auto isolation_image = ui::Image::decode(kMipIsolationPng);
    if (!isolation_image.valid()) {
        return example::fail("T085 mip isolation fixture did not decode");
    }
    constexpr std::array<ui::TextureTileMode, 4> modes{
        ui::TextureTileMode::Clamp,
        ui::TextureTileMode::Repeat,
        ui::TextureTileMode::Mirror,
        ui::TextureTileMode::Decal,
    };
    constexpr std::array<ui::TextureMipmap, 2> mipmaps{
        ui::TextureMipmap::Nearest,
        ui::TextureMipmap::Linear,
    };
    for (const auto mode : modes) {
        for (const auto filter : filters) {
            for (const auto mipmap : mipmaps) {
                ui::ImageTexture texture{
                    isolation_image,
                    {4.0f, 4.0f, 8.0f, 8.0f},
                    {8.0f, 8.0f, 2.0f, 2.0f}};
                texture.set_tile_mode(mode, mode);
                texture.set_sampling(sampling(filter, mipmap));
                const auto rendered = render_texture(
                    texture, 20, 20, {1.0f, 0.0f, 1.0f, 1.0f});
                if (rendered.pixels.empty()) {
                    return example::fail("T085 mip source-isolation render failed");
                }
                if (!red(rendered.at(8, 8))) {
                    return example::fail("T085 selected source bled parent pixels into mip levels");
                }
                if (mode == ui::TextureTileMode::Decal) {
                    if (!magenta(rendered.at(2, 2)) || !magenta(rendered.at(15, 15))) {
                        return example::fail("T085 Decal mip sampling escaped destination period");
                    }
                } else if (!red(rendered.at(2, 2)) || !red(rendered.at(15, 15))) {
                    return example::fail("T085 tiled mip sampling bled outside selected source");
                }
            }
        }
    }

    // Fractional selected-source + mip-enabled magnification must still sample
    // the isolated level 0, not the parent image. Both filters are exercised.
    for (const auto filter : filters) {
        ui::ImageTexture fractional{
            isolation_image,
            {4.25f, 4.25f, 7.5f, 7.5f},
            {2.0f, 2.0f, 15.0f, 15.0f}};
        fractional.set_sampling(sampling(filter, ui::TextureMipmap::Linear));
        const auto rendered = render_texture(
            fractional, 20, 20, {1.0f, 0.0f, 1.0f, 1.0f});
        if (rendered.pixels.empty() || !red(rendered.at(9, 9))) {
            return example::fail("T085 fractional mip level-0 isolation failed");
        }
    }

    // Extreme minification must stay bounded and reusable across repeated
    // warmed draws. Image decode happened before either render and the public
    // value is unchanged by materialization.
    ui::ImageTexture extreme{
        isolation_image,
        {4.0f, 4.0f, 8.0f, 8.0f},
        {0.0f, 0.0f, 1.0f, 1.0f}};
    extreme.set_tile_mode(ui::TextureTileMode::Repeat, ui::TextureTileMode::Repeat);
    extreme.set_sampling(sampling(ui::TextureFilter::Linear, ui::TextureMipmap::Linear));
    const ui::Brush extreme_brush{extreme};
    ui::UI repeated_tree{ui::Canvas{16.0f, 16.0f, [extreme_brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 16.0f, 16.0f}, extreme_brush);
    }}};
    ui::HeadlessRenderer repeated_renderer{{16.0f, 16.0f}, 1.0f};
    if (!repeated_renderer.render(repeated_tree) ||
        !repeated_renderer.render(repeated_tree) ||
        !red(repeated_renderer.pixel(8, 8))) {
        return example::fail("T085 repeated extreme-minification draw failed");
    }

    return 0;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(
                application.last_error().empty()
                    ? "T085 platform application is invalid"
                    : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = make_demo_ui();
        if (!standalone_ui) return example::fail("T085 smoke image did not decode");
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T085 platform smoke",
                .size = {400.0f, 220.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(
                standalone.last_error().empty()
                    ? "T085 standalone window is invalid"
                    : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = make_demo_ui();
        if (!embedded_ui) return example::fail("T085 embedded smoke image did not decode");
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {360.0f, 180.0f}};
        if (!embedded.native_handle()) {
            return example::fail(
                embedded.last_error().empty()
                    ? "T085 embedded view is invalid"
                    : embedded.last_error());
        }

        stage = "native-paint";
        for (int i = 0; i < 12; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) {
            return example::fail(standalone.last_error());
        }
        if (!embedded.last_error().empty()) {
            return example::fail(embedded.last_error());
        }
        return 0;
    } catch (const std::exception& error) {
        return example::fail(
            std::string{"T085 platform smoke "} + stage + ": " + error.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") {
        return platform_smoke();
    }

    auto tree = make_demo_ui();
    if (!tree) return 1;
    return example::run_window(
        *tree, "NativeUI T085 Texture Sampling", {400.0f, 220.0f});
}
