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
            // Tile a genuinely minified one-pixel period so native GPU smoke
            // exercises mipmap generation/selection as well as tiling.
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

    ui::ImageTexture nearest_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 32.0f, 32.0f}};
    if (nearest_texture.sampling().filter() != ui::TextureFilter::Linear ||
        nearest_texture.sampling().mipmap() != ui::TextureMipmap::None) {
        return example::fail("T085 ImageTexture defaults changed T083 behavior");
    }
    nearest_texture.set_sampling(
        sampling(ui::TextureFilter::Nearest, ui::TextureMipmap::None));

    ui::ImageTexture mip_texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {36.0f, 0.0f, 1.0f, 1.0f}};
    mip_texture.set_sampling(
        sampling(ui::TextureFilter::Linear, ui::TextureMipmap::Linear));

    const ui::Brush nearest{nearest_texture};
    const ui::Brush mip{mip_texture};
    ui::UI tree{ui::Canvas{40.0f, 32.0f, [nearest, mip](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, nearest);
        g.fill_rect({36.0f, 0.0f, 1.0f, 1.0f}, mip);
    }}};
    ui::HeadlessRenderer renderer{{40.0f, 32.0f}, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("T085 headless sampling render failed");
    }
    if (!red(renderer.pixel(4, 4)) || !green(renderer.pixel(27, 4))) {
        return example::fail("T085 nearest filtering samples are incorrect");
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

        // Force Nearest/Linear + mipmap sampling through native Ganesh/OpenGL
        // in two independent view/resource contexts.
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
