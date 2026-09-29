#include "src/detail/platform_test_access.hpp"
#include "src/detail/scalar_source_access.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/nativeui.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::array<std::byte, 70> kDataPng{
    std::byte{137},std::byte{80},std::byte{78},std::byte{71},std::byte{13},std::byte{10},std::byte{26},std::byte{10},
    std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{72},std::byte{68},std::byte{82},
    std::byte{0},std::byte{0},std::byte{0},std::byte{1},std::byte{0},std::byte{0},std::byte{0},std::byte{1},
    std::byte{8},std::byte{6},std::byte{0},std::byte{0},std::byte{0},std::byte{31},std::byte{21},std::byte{196},
    std::byte{137},std::byte{0},std::byte{0},std::byte{0},std::byte{13},std::byte{73},std::byte{68},std::byte{65},
    std::byte{84},std::byte{120},std::byte{218},std::byte{99},std::byte{56},std::byte{145},std::byte{98},std::byte{228},
    std::byte{0},std::byte{0},std::byte{4},std::byte{245},std::byte{1},std::byte{159},std::byte{91},std::byte{144},
    std::byte{228},std::byte{44},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{73},std::byte{69},
    std::byte{78},std::byte{68},std::byte{174},std::byte{66},std::byte{96},std::byte{130},
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

std::string_view channel_expression(ui::ScalarChannel channel) {
    switch (channel) {
    case ui::ScalarChannel::Red:
        return "value.r";
    case ui::ScalarChannel::Green:
        return "value.g";
    case ui::ScalarChannel::Blue:
        return "value.b";
    case ui::ScalarChannel::Alpha:
        return "value.a";
    }
    throw std::runtime_error{"invalid ScalarSource GPU channel"};
}

ui::Brush scalar_brush(const ui::ScalarSource& source) {
    if (ui::detail::ScalarSourceAccess::is_constant(source)) {
        const auto compiled = ui::ShaderProgram::compile(R"(
            uniform float scalar;
            half4 main(float2) {
                return half4(scalar, scalar, scalar, 1.0);
            }
        )");
        check(compiled.ok(), "constant ScalarSource GPU probe compile failed");
        ui::ShaderInstance shader{compiled.program};
        check(shader.set_float(
                  "scalar",
                  ui::detail::ScalarSourceAccess::constant_value(source)) ==
                  ui::ShaderSetResult::Ok,
              "constant ScalarSource GPU probe binding failed");
        return ui::Brush{shader};
    }

    const auto* brush = ui::detail::ScalarSourceAccess::brush(source);
    check(brush != nullptr, "ScalarSource GPU probe lost Brush storage");

    std::string sksl{
        "uniform shader source;\n"
        "half4 main(float2 p) {\n"
        "    half4 value = source.eval(p);\n"
        "    half scalar = "};
    sksl += channel_expression(ui::detail::ScalarSourceAccess::channel(source));
    sksl +=
        ";\n"
        "    return half4(scalar, scalar, scalar, 1.0);\n"
        "}\n";

    const auto compiled = ui::ShaderProgram::compile(sksl);
    check(compiled.ok(), "ScalarSource GPU probe compile failed");
    ui::ShaderInstance shader{compiled.program};
    check(shader.set_child("source", *brush) == ui::ShaderSetResult::Ok,
          "ScalarSource GPU probe child binding failed");
    return ui::Brush{shader};
}

ui::UI make_ui(const ui::Brush& brush) {
    return ui::UI{ui::Canvas{
        64.0f,
        64.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, brush);
        }}};
}

bool near_channel(std::uint8_t actual,
                  std::uint8_t expected,
                  int tolerance = 5) noexcept {
    return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <=
           tolerance;
}

void compare_gpu(ui::Application& application,
                 const ui::ScalarSource& source,
                 const char* title) {
    const auto brush = scalar_brush(source);

    auto reference_ui = make_ui(brush);
    ui::HeadlessRenderer reference{{64.0f, 64.0f}, 1.0f};
    check(reference.render(reference_ui),
          "ScalarSource headless reference render failed");
    const auto expected = reference.pixel(32, 32);

    auto gpu_ui = make_ui(brush);
    ui::StandaloneWindow window{
        application,
        gpu_ui,
        ui::WindowDesc{
            .title = title,
            .size = {64.0f, 64.0f},
            .resizable = false}};
    check(window.valid() && window.native_handle(),
          "ScalarSource GPU window is invalid");

    check(ui::detail::PlatformTestAccess::request_gpu_readback(
              window, ui::Point{32.0f, 32.0f}),
          "ScalarSource GPU readback request was rejected");

    std::optional<ui::detail::PlatformReadbackPixel> actual;
    for (int iteration = 0; iteration < 32 && !actual; ++iteration) {
        (void)application.poll(0.0);
        actual = ui::detail::PlatformTestAccess::take_gpu_readback(window);
    }
    check(actual.has_value(), "ScalarSource GPU readback did not complete");
    check(near_channel(actual->r, expected.r) &&
              near_channel(actual->g, expected.g) &&
              near_channel(actual->b, expected.b) &&
              near_channel(actual->a, expected.a),
          "ScalarSource GPU result diverged from headless reference");
}

ui::ScalarSource data_source() {
    const auto image = ui::Image::decode(kDataPng);
    check(image.valid(), "ScalarSource GPU Data fixture decode failed");
    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 64.0f, 64.0f}};
    ui::TextureSampling sampling;
    sampling.set_filter(ui::TextureFilter::Nearest)
            .set_mipmap(ui::TextureMipmap::None);
    texture.set_sampling(sampling)
           .set_interpretation(ui::TextureInterpretation::Data);
    return ui::ScalarSource::from_brush(
        ui::Brush{texture}, ui::ScalarChannel::Green);
}

ui::ScalarSource shader_source() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) {
            return half4(0.15, 0.35, 0.75, 1.0);
        }
    )");
    check(compiled.ok(), "ScalarSource GPU shader fixture compile failed");
    ui::ShaderInstance shader{compiled.program};
    return ui::ScalarSource::from_brush(
        ui::Brush{shader}, ui::ScalarChannel::Blue);
}

ui::ScalarSource noise_source() {
    const auto created = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 28.0f, .seed = 0x12345678u});
    check(created.ok(), "ScalarSource GPU noise fixture compile failed");
    return ui::ScalarSource::from_noise(created.noise);
}

} // namespace

int main() {
    try {
        ui::Application application;
        check(application.valid(), "ScalarSource GPU application is invalid");
        application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

        const auto solid = ui::ScalarSource::from_brush(
            ui::Brush{ui::Color{0.15f, 0.65f, 0.25f, 1.0f}},
            ui::ScalarChannel::Green);
        compare_gpu(application, solid, "NativeUI ScalarSource solid");
        compare_gpu(application, data_source(), "NativeUI ScalarSource data");
        compare_gpu(application, shader_source(), "NativeUI ScalarSource shader");
        compare_gpu(application, noise_source(), "NativeUI ScalarSource noise");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL ScalarSource GPU reference: "
                  << error.what() << '\n';
        return 1;
    }
}
