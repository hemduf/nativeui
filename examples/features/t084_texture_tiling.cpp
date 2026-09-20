#include "example_support.hpp"

#include <nativeui/headless.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

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

[[nodiscard]] bool green(ui::Rgba8 p) noexcept {
    return p.g > 180 && p.r < 70 && p.b < 70;
}

[[nodiscard]] bool blue(ui::Rgba8 p) noexcept {
    return p.b > 180 && p.r < 70 && p.g < 70;
}

[[nodiscard]] std::unique_ptr<ui::UI> make_demo_ui() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return {};

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {24.0f, 24.0f, 32.0f, 32.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    const ui::Brush brush{texture};

    return std::make_unique<ui::UI>(
        ui::Canvas{320.0f, 180.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect(
                {0.0f, 0.0f, 320.0f, 180.0f},
                {0.06f, 0.06f, 0.07f, 1.0f});
            g.fill_rounded_rect(
                {24.0f, 20.0f, 272.0f, 140.0f}, 14.0f, brush);
        }});
}

int self_test() {
    const auto image = ui::Image::decode(kTinyRgbaPng);
    if (!image.valid()) return example::fail("T084 sample image did not decode");

    ui::ImageTexture texture{
        image, {0.0f, 0.0f, 2.0f, 2.0f}, {16.0f, 16.0f, 16.0f, 16.0f}};
    texture.set_tile_mode(
        ui::TextureTileMode::Repeat, ui::TextureTileMode::Mirror);
    const ui::Brush brush{texture};

    ui::UI tree{ui::Canvas{48.0f, 48.0f, [brush](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, 48.0f, 48.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        g.fill_rect({0.0f, 0.0f, 48.0f, 48.0f}, brush);
    }}};
    ui::HeadlessRenderer renderer{{48.0f, 48.0f}, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("T084 headless tiled render failed");
    }

    // (-.25,-.25) normalizes to (.75,.25): top-right (green).
    if (!green(renderer.pixel(12, 12))) {
        return example::fail("T084 negative Repeat/Mirror sample is incorrect");
    }
    // (1.25,1.25) normalizes to (.25,.75): bottom-left (blue).
    if (!blue(renderer.pixel(36, 36))) {
        return example::fail("T084 positive Repeat/Mirror sample is incorrect");
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
                    ? "T084 platform application is invalid"
                    : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = make_demo_ui();
        if (!standalone_ui) return example::fail("T084 smoke image did not decode");
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T084 platform smoke",
                .size = {360.0f, 220.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(
                standalone.last_error().empty()
                    ? "T084 standalone window is invalid"
                    : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = make_demo_ui();
        if (!embedded_ui) return example::fail("T084 embedded smoke image did not decode");
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {320.0f, 180.0f}};
        if (!embedded.native_handle()) {
            return example::fail(
                embedded.last_error().empty()
                    ? "T084 embedded view is invalid"
                    : embedded.last_error());
        }

        // Force Repeat/Mirror ImageTexture materialization through the native
        // Ganesh/OpenGL path in two independent view/resource contexts.
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
            std::string{"T084 platform smoke "} + stage + ": " + error.what());
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
        *tree, "NativeUI T084 ImageTexture Tiling", {360.0f, 220.0f});
}
