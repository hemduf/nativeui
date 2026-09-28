#include "example_support.hpp"

#include <nativeui/headless.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

[[nodiscard]] std::optional<ui::Brush> make_mixed_brush(
    const ui::ImageTexture& color,
    const ui::ImageTexture& data) {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader color_source;
        uniform shader data_source;
        half4 main(float2 p) {
            half4 c = color_source.eval(p);
            half4 d = data_source.eval(p);
            return half4(c.r * 0.5, d.g, d.b, 1.0);
        }
    )");
    if (!compiled.ok()) return std::nullopt;

    ui::ShaderInstance instance{compiled.program};
    if (instance.set_child("color_source", ui::Brush{color}) !=
            ui::ShaderSetResult::Ok ||
        instance.set_child("data_source", ui::Brush{data}) !=
            ui::ShaderSetResult::Ok) {
        return std::nullopt;
    }
    return ui::Brush{instance};
}

[[nodiscard]] std::unique_ptr<ui::UI> make_demo_ui() {
    const auto image = ui::Image::decode(kTaggedPng);
    if (!image.valid()) return {};

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {20.0f, 20.0f, 160.0f, 160.0f}};
    ui::ImageTexture data{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {200.0f, 20.0f, 160.0f, 160.0f}};
    data.set_interpretation(ui::TextureInterpretation::Data);

    const ui::Brush color_brush{color};
    const ui::Brush data_brush{data};
    const auto mixed_brush = make_mixed_brush(color, data);
    if (!mixed_brush) return {};

    return std::make_unique<ui::UI>(ui::Canvas{
        560.0f,
        200.0f,
        [color_brush, data_brush, mixed_brush = *mixed_brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 560.0f, 200.0f}, ui::colors::panel);
            g.fill_rect({20.0f, 20.0f, 160.0f, 160.0f}, color_brush);
            g.fill_rect({200.0f, 20.0f, 160.0f, 160.0f}, data_brush);
            g.fill_rect({380.0f, 20.0f, 160.0f, 160.0f}, mixed_brush);
        }});
}

int self_test() {
    const auto image = ui::Image::decode(kTaggedPng);
    if (!image.valid()) return example::fail("tagged image did not decode");

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {20.0f, 20.0f, 160.0f, 160.0f}};
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

    const auto mixed = make_mixed_brush(color, data);
    if (!mixed) return example::fail("mixed Color/Data shader setup failed");

    ui::UI tree{ui::Canvas{
        32.0f, 32.0f, [mixed = *mixed](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, mixed);
        }}};
    ui::HeadlessRenderer renderer{{32.0f, 32.0f}, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("mixed Color/Data headless render failed");
    }

    const auto pixel = renderer.pixel(16, 16);
    const auto near = [](std::uint8_t actual, int expected) noexcept {
        const int value = static_cast<int>(actual);
        return value >= expected - 4 && value <= expected + 4;
    };
    // The fixture is tagged linear-sRGB. Color R is halved in linear-sRGB;
    // raw Data G/B remain numeric, then the mixed effect is encoded for sRGB.
    if (!near(pixel.r, 137) || !near(pixel.g, 137) ||
        !near(pixel.b, 99) || !near(pixel.a, 255)) {
        return example::fail("mixed Color/Data self-test pixel mismatch");
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
                    ? "ImageTexture platform application is invalid"
                    : application.last_error());
        }

        stage = "standalone";
        auto standalone_ui = make_demo_ui();
        if (!standalone_ui) return example::fail("ImageTexture smoke image did not decode");
        ui::StandaloneWindow standalone{
            application,
            *standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI ImageTexture interpretation smoke",
                .size = {600.0f, 240.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(
                standalone.last_error().empty()
                    ? "ImageTexture standalone window is invalid"
                    : standalone.last_error());
        }

        stage = "embedded";
        auto embedded_ui = make_demo_ui();
        if (!embedded_ui) return example::fail("ImageTexture embedded smoke image did not decode");
        ui::EmbeddedView embedded{
            *embedded_ui, standalone.native_handle(), {560.0f, 200.0f}};
        if (!embedded.native_handle()) {
            return example::fail(
                embedded.last_error().empty()
                    ? "ImageTexture embedded view is invalid"
                    : embedded.last_error());
        }

        // Exercise ordinary color management, raw Data sampling, and the
        // mixed Color/Data linear-sRGB runtime-effect path through two
        // independent Ganesh/OpenGL renderer contexts.
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
            std::string{"ImageTexture platform smoke "} + stage + ": " + error.what());
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
        *tree, "ImageTexture Color vs Data", {600.0f, 240.0f});
}
