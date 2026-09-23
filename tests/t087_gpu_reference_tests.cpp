#include "src/detail/platform_test_access.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/nativeui.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>

namespace {

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

[[nodiscard]] int fail(const char* message) {
    std::cerr << "FAIL T087 GPU reference: " << message << '\n';
    return 1;
}

[[nodiscard]] bool near_channel(
    std::uint8_t actual,
    std::uint8_t expected,
    int tolerance = 4) noexcept {
    return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <=
           tolerance;
}

[[nodiscard]] ui::Brush mixed_brush() {
    const auto image = ui::Image::decode(kUntaggedPng);
    if (!image.valid()) {
        throw std::runtime_error("fixture decode failed");
    }

    ui::ImageTexture color{
        image, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 64.0f, 64.0f}};
    auto data = color;
    data.set_interpretation(ui::TextureInterpretation::Data);

    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform shader color_source;
        uniform shader data_source;
        half4 main(float2 p) {
            half4 c = color_source.eval(p);
            half4 d = data_source.eval(p);
            return half4(c.r * 0.5, d.g, d.b, 1.0);
        }
    )");
    if (!compiled.ok()) {
        throw std::runtime_error("mixed shader compile failed");
    }

    ui::ShaderInstance instance{compiled.program};
    if (instance.set_child("color_source", ui::Brush{color}) !=
            ui::ShaderSetResult::Ok ||
        instance.set_child("data_source", ui::Brush{data}) !=
            ui::ShaderSetResult::Ok) {
        throw std::runtime_error("mixed shader child binding failed");
    }
    return ui::Brush{instance};
}

[[nodiscard]] ui::UI make_ui(const ui::Brush& brush) {
    return ui::UI{ui::Canvas{
        64.0f,
        64.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, brush);
        }}};
}

} // namespace

int main() {
    try {
        const auto brush = mixed_brush();

        auto reference_ui = make_ui(brush);
        ui::HeadlessRenderer reference{{64.0f, 64.0f}, 1.0f};
        if (!reference.render(reference_ui)) {
            return fail("headless reference render failed");
        }
        const auto expected = reference.pixel(32, 32);

        // The controlled fixture has sRGB bytes (128,64,32). Color R is
        // decoded to linear-sRGB before the 0.5 multiply, while Data G/B stay
        // numeric. The sRGB reference surface therefore lands near
        // (92,137,99). Keep an analytic oracle in addition to CPU/GPU parity.
        if (!near_channel(expected.r, 92) ||
            !near_channel(expected.g, 137) ||
            !near_channel(expected.b, 99) ||
            !near_channel(expected.a, 255)) {
            return fail("headless analytic oracle mismatch");
        }

        ui::Application application;
        if (!application.valid()) {
            return fail("platform application is invalid");
        }

        auto gpu_ui = make_ui(brush);
        ui::StandaloneWindow window{
            application,
            gpu_ui,
            ui::WindowDesc{
                .title = "NativeUI T087 GPU reference",
                .size = {64.0f, 64.0f},
                .resizable = false}};
        if (!window.valid() || !window.native_handle()) {
            return fail("platform window is invalid");
        }

        if (!ui::detail::PlatformTestAccess::request_gpu_readback(
                window, ui::Point{32.0f, 32.0f})) {
            return fail("GPU readback request was rejected");
        }

        std::optional<ui::detail::PlatformReadbackPixel> actual;
        for (int iteration = 0; iteration < 32 && !actual; ++iteration) {
            (void)application.poll(0.0);
            actual = ui::detail::PlatformTestAccess::take_gpu_readback(window);
        }
        if (!actual) return fail("GPU readback did not complete");

        if (!near_channel(actual->r, expected.r) ||
            !near_channel(actual->g, expected.g) ||
            !near_channel(actual->b, expected.b) ||
            !near_channel(actual->a, expected.a)) {
            return fail("GPU result diverged from headless reference");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL T087 GPU reference: " << error.what() << '\n';
        return 1;
    }
}
