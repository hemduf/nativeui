#include <nativeui/noise.hpp>
#include <nativeui/scalar_source.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/scalar_source_access.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

void source_creation_contract() {
    const auto initial = ui::detail::shader_compile_call_count_for_test();

    ui::ScalarSource zero;
    ui::ScalarSource direct{8.0f};
    auto constant = ui::ScalarSource::constant(-2.0f);
    (void)zero;
    (void)direct;
    (void)constant;
    check(ui::detail::shader_compile_call_count_for_test() == initial,
          "constant ScalarSource construction compiled source");

    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) { return half4(0.25, 0.5, 0.75, 1.0); }
    )");
    check(compiled.ok(), "shader setup failed");
    const auto after_shader_compile =
        ui::detail::shader_compile_call_count_for_test();
    check(after_shader_compile == initial + 1U,
          "shader setup did not compile exactly once");

    ui::ShaderInstance shader{compiled.program};
    const ui::Brush shader_brush{shader};

    auto brush_source =
        ui::ScalarSource::from_brush(shader_brush, ui::ScalarChannel::Green);
    check(ui::detail::ScalarSourceAccess::brush(brush_source) != nullptr,
          "Brush-backed ScalarSource lost its source");
    check(ui::detail::shader_compile_call_count_for_test() ==
              after_shader_compile,
          "from_brush recompiled an existing source");

    const auto created = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 32.0f, .seed = 0x12345678u});
    check(created.ok(), "noise setup failed");
    const auto after_noise_compile =
        ui::detail::shader_compile_call_count_for_test();
    check(after_noise_compile == after_shader_compile + 1U,
          "NoiseSource setup did not compile exactly once");

    auto noise_source = ui::ScalarSource::from_noise(created.noise);
    check(ui::detail::ScalarSourceAccess::brush(noise_source) != nullptr,
          "NoiseSource conversion lost its immutable source");
    check(ui::detail::shader_compile_call_count_for_test() ==
              after_noise_compile,
          "from_noise recompiled an existing NoiseSource");

    ui::NoiseSource inert;
    auto inert_scalar = ui::ScalarSource::from_noise(inert);
    const auto* inert_brush =
        ui::detail::ScalarSourceAccess::brush(inert_scalar);
    check(inert_brush != nullptr,
          "inert NoiseSource did not produce a stable scalar source");
    check(ui::detail::ShaderBrushAccess::is_transparent_solid(*inert_brush),
          "inert NoiseSource did not preserve scalar zero");
    check(ui::detail::shader_compile_call_count_for_test() ==
              after_noise_compile,
          "inert NoiseSource conversion retried compilation");
}


std::shared_ptr<const ui::ShaderProgram> compile_probe(std::string_view source) {
    const auto compiled = ui::ShaderProgram::compile(source);
    check(compiled.ok() && compiled.program != nullptr,
          "ScalarSource probe shader compilation failed");
    return compiled.program;
}

std::string channel_expression(ui::ScalarChannel channel) {
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
    throw std::runtime_error{"invalid ScalarSource probe channel"};
}

ui::Brush select_channel(const ui::Brush& brush, ui::ScalarChannel channel) {
    std::string source{
        "uniform shader source;\n"
        "half4 main(float2 p) {\n"
        "    half4 value = source.eval(p);\n"
        "    half scalar = "};
    source += channel_expression(channel);
    source +=
        ";\n"
        "    return half4(scalar, scalar, scalar, 1.0);\n"
        "}\n";

    ui::ShaderInstance probe{compile_probe(source)};
    check(probe.set_child("source", brush) == ui::ShaderSetResult::Ok,
          "ScalarSource probe child binding failed");
    return ui::Brush{probe};
}

ui::Brush scalar_probe(const ui::ScalarSource& source) {
    if (ui::detail::ScalarSourceAccess::is_constant(source)) {
        auto program = compile_probe(R"(
            uniform float scalar;
            half4 main(float2) {
                return half4(scalar, scalar, scalar, 1.0);
            }
        )");
        ui::ShaderInstance probe{std::move(program)};
        check(probe.set_float(
                  "scalar",
                  ui::detail::ScalarSourceAccess::constant_value(source)) ==
                  ui::ShaderSetResult::Ok,
              "ScalarSource constant probe binding failed");
        return ui::Brush{probe};
    }

    const auto* brush = ui::detail::ScalarSourceAccess::brush(source);
    check(brush != nullptr, "ScalarSource probe lost Brush storage");
    return select_channel(
        *brush, ui::detail::ScalarSourceAccess::channel(source));
}

std::array<float, 4> render_probe(const ui::Brush& brush,
                                  int x,
                                  int y,
                                  float translate_x = 0.0f) {
    const auto info = SkImageInfo::Make(
        24,
        16,
        kRGBA_F32_SkColorType,
        kPremul_SkAlphaType,
        SkColorSpace::MakeSRGBLinear());
    auto surface = SkSurfaces::Raster(info);
    check(surface != nullptr, "ScalarSource probe surface creation failed");
    auto* canvas = surface->getCanvas();
    check(canvas != nullptr, "ScalarSource probe canvas missing");
    canvas->clear(SK_ColorTRANSPARENT);

    ui::Painter painter{*canvas};
    if (translate_x == 0.0f) {
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, brush);
    } else {
        auto state = painter.scoped_state();
        painter.translate(translate_x, 0.0f);
        painter.fill_rounded_rect(
            {0.0f, 0.0f, 16.0f, 16.0f}, 0.0f, brush);
    }

    SkPixmap pixmap;
    check(surface->peekPixels(&pixmap), "ScalarSource probe pixels missing");
    const auto value = pixmap.getColor4f(x, y);
    return {value.fR, value.fG, value.fB, value.fA};
}

void check_pixel_near(const std::array<float, 4>& actual,
                      const std::array<float, 4>& expected,
                      float tolerance = 0.0025f) {
    for (std::size_t index = 0; index < actual.size(); ++index) {
        check(std::abs(actual[index] - expected[index]) <= tolerance,
              "ScalarSource probe pixel mismatch");
    }
}

void brush_sampling_contract() {
    const ui::Brush solid{
        ui::Color{0.2f, 0.4f, 0.7f, 1.0f}};
    const ui::Brush gradient{ui::LinearGradient{
        {0.0f, 0.0f},
        {16.0f, 0.0f},
        ui::Color{0.1f, 0.2f, 0.3f, 1.0f},
        ui::Color{0.8f, 0.6f, 0.4f, 1.0f}}};

    const auto shader_program = compile_probe(R"(
        half4 main(float2 p) {
            return half4(-0.25, 1.5, p.x / 16.0, 0.75);
        }
    )");
    ui::ShaderInstance shader{shader_program};
    const ui::Brush shader_brush{shader};

    for (const auto& brush : {solid, gradient, shader_brush}) {
        for (auto channel : {
                 ui::ScalarChannel::Red,
                 ui::ScalarChannel::Green,
                 ui::ScalarChannel::Blue,
                 ui::ScalarChannel::Alpha}) {
            const auto scalar = ui::ScalarSource::from_brush(brush, channel);
            const auto actual = scalar_probe(scalar);
            const auto expected = select_channel(brush, channel);
            check_pixel_near(
                render_probe(actual, 3, 7),
                render_probe(expected, 3, 7));
            check_pixel_near(
                render_probe(actual, 12, 7),
                render_probe(expected, 12, 7));
            check_pixel_near(
                render_probe(actual, 7, 7, 2.0f),
                render_probe(expected, 7, 7, 2.0f));
        }
    }
}

void snapshot_lifetime_contract() {
    const auto scalar = [] {
        ui::Brush temporary{ui::LinearGradient{
            {0.0f, 0.0f},
            {16.0f, 0.0f},
            ui::Color{0.0f, 0.0f, 0.0f, 1.0f},
            ui::Color{1.0f, 1.0f, 1.0f, 1.0f}}};
        return ui::ScalarSource::from_brush(
            std::move(temporary), ui::ScalarChannel::Red);
    }();

    const auto brush = scalar_probe(scalar);
    const auto left = render_probe(brush, 2, 8);
    const auto right = render_probe(brush, 13, 8);
    check(right[0] > left[0] + 0.35f,
          "ScalarSource did not retain the temporary Brush snapshot");
}

void noise_sampling_contract() {
    for (auto type : {
             ui::NoiseType::Value,
             ui::NoiseType::Perlin,
             ui::NoiseType::Simplex}) {
        const auto created = ui::NoiseSource::create(
            type, {.feature_size = 24.0f, .seed = 0x12345678u});
        check(created.ok(), "ScalarSource noise setup failed");

        const auto scalar = ui::ScalarSource::from_noise(created.noise);
        const auto actual = scalar_probe(scalar);
        const auto expected = select_channel(
            created.noise.as_brush(
                {0.0f, 0.0f, 0.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f}),
            ui::ScalarChannel::Red);

        check_pixel_near(
            render_probe(actual, 4, 4),
            render_probe(expected, 4, 4));
        check_pixel_near(
            render_probe(actual, 11, 9),
            render_probe(expected, 11, 9));
    }

    ui::FractalNoiseOptions fractal_options;
    fractal_options.set_mode(ui::FractalNoiseMode::Ridged)
                   .set_octaves(4)
                   .set_lacunarity(2.0f)
                   .set_gain(0.5f);
    const auto fractal = ui::NoiseSource::create_fractal(
        ui::NoiseType::Simplex,
        {.feature_size = 32.0f, .seed = 0xA5A5A5A5u},
        fractal_options);
    check(fractal.ok(), "ScalarSource fractal setup failed");

    const auto scalar = ui::ScalarSource::from_noise(fractal.noise);
    const auto actual = scalar_probe(scalar);
    const auto expected = select_channel(
        fractal.noise.as_brush(
            {0.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f}),
        ui::ScalarChannel::Red);
    check_pixel_near(
        render_probe(actual, 8, 6),
        render_probe(expected, 8, 6));
}

void independent_source_contract() {
    auto first = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.8f, 0.1f, 0.1f, 1.0f}},
        ui::ScalarChannel::Red);
    auto second = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.1f, 0.7f, 0.1f, 1.0f}},
        ui::ScalarChannel::Green);

    const auto second_before = render_probe(scalar_probe(second), 8, 8);
    {
        auto copy = first;
        const auto first_pixel = render_probe(scalar_probe(copy), 8, 8);
        check(first_pixel[0] > 0.6f,
              "ScalarSource first instance did not render independently");
    }
    const auto second_after = render_probe(scalar_probe(second), 8, 8);
    check_pixel_near(second_after, second_before);
}

} // namespace

int main() {
    source_creation_contract();
    brush_sampling_contract();
    snapshot_lifetime_contract();
    noise_sampling_contract();
    independent_source_contract();
    return 0;
}
