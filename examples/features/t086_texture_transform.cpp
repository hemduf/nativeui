#include "example_support.hpp"

#include <nativeui/headless.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
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

[[nodiscard]] ui::TextureSampling nearest() noexcept {
    ui::TextureSampling value;
    value.set_filter(ui::TextureFilter::Nearest)
         .set_mipmap(ui::TextureMipmap::None);
    return value;
}

[[nodiscard]] ui::ImageTexture make_texture(const ui::Image& image) {
    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 48.0f, 48.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Decal, ui::TextureTileMode::Decal);
    texture.set_sampling(nearest());
    return texture;
}

[[nodiscard]] std::unique_ptr<ui::UI> make_demo_ui() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return {};

    auto identity = make_texture(image);
    identity.set_transform(ui::Transform2D::translation(24.0f, 28.0f));

    auto rotated = make_texture(image);
    rotated.set_transform(
        ui::Transform2D::translation(176.0f, 28.0f) *
        ui::Transform2D::rotation(0.35f));

    auto sheared = make_texture(image);
    sheared.set_transform(
        ui::Transform2D::translation(290.0f, 28.0f) *
        ui::Transform2D{1.0f, 0.45f, 0.0f, 0.0f, 1.0f, 0.0f});

    return std::make_unique<ui::UI>(
        ui::Canvas{380.0f, 120.0f,
            [a = ui::Brush{identity},
             b = ui::Brush{rotated},
             c = ui::Brush{sheared}](ui::CanvasContext2D& g) {
                g.fill_rect(
                    {0.0f, 0.0f, 380.0f, 120.0f},
                    {0.06f, 0.06f, 0.07f, 1.0f});
                g.fill_rect({0.0f, 0.0f, 380.0f, 120.0f}, a);
                g.fill_rect({0.0f, 0.0f, 380.0f, 120.0f}, b);
                g.fill_rect({0.0f, 0.0f, 380.0f, 120.0f}, c);
            }});
}

int self_test() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return example::fail("T086 sample image did not decode");

    auto texture = make_texture(image);
    if (!texture.valid() ||
        texture.transform().m00 != 1.0f ||
        texture.transform().m11 != 1.0f) {
        return example::fail("T086 identity default changed T083 behavior");
    }

    const auto transform =
        ui::Transform2D::translation(16.0f, 8.0f) *
        ui::Transform2D{1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f};
    if (&texture.set_transform(transform) != &texture ||
        !texture.valid() ||
        texture.transform().m01 != 0.5f ||
        texture.transform().m02 != 16.0f ||
        texture.transform().m12 != 8.0f) {
        return example::fail("T086 transform roundtrip failed");
    }

    const ui::Brush snapshot{texture};
    texture.set_transform(ui::Transform2D::translation(96.0f, 8.0f));
    const ui::Brush updated{texture};

    ui::UI tree{ui::Canvas{160.0f, 80.0f, [snapshot, updated](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 160.0f, 80.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        g.fill_rect({0.0f, 0.0f, 160.0f, 80.0f}, snapshot);
        g.fill_rect({0.0f, 0.0f, 160.0f, 80.0f}, updated);
    }}};
    ui::HeadlessRenderer renderer{{160.0f, 80.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("T086 transformed render failed");

    const auto old_pixel = renderer.pixel(22, 14);
    const auto new_pixel = renderer.pixel(102, 14);
    if (old_pixel.r < 160 || new_pixel.r < 160) {
        return example::fail("T086 valid transform/snapshot pixels are missing");
    }

    const float inf = std::numeric_limits<float>::infinity();
    const ui::Transform2D invalid{1.0f, 0.0f, inf, 0.0f, 1.0f, 0.0f};
    texture.set_transform(invalid);
    if (texture.valid() || texture.image() != image ||
        texture.transform().m02 != inf) {
        return example::fail("T086 invalid transform did not preserve logical state");
    }

    texture.set_transform(ui::Transform2D::identity());
    if (!texture.valid()) {
        return example::fail("T086 invalid-to-valid recovery failed");
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
                    ? "T086 platform application is invalid"
                    : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = make_demo_ui();
        if (!standalone_ui) return example::fail("T086 smoke image did not decode");
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T086 platform smoke",
                .size = {420.0f, 170.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(
                standalone.last_error().empty()
                    ? "T086 standalone window is invalid"
                    : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = make_demo_ui();
        if (!embedded_ui) return example::fail("T086 embedded smoke image did not decode");
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {380.0f, 120.0f}};
        if (!embedded.native_handle()) {
            return example::fail(
                embedded.last_error().empty()
                    ? "T086 embedded view is invalid"
                    : embedded.last_error());
        }

        stage = "native-paint";
        for (int i = 0; i < 12; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());
        return 0;
    } catch (const std::exception& error) {
        return example::fail(
            std::string{"T086 platform smoke "} + stage + ": " + error.what());
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
        *tree, "NativeUI T086 Texture Transform", {420.0f, 170.0f});
}
