#include <nativeui/headless.hpp>
#include <nativeui/image.hpp>
#include <nativeui/noise.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/scalar_source.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/image_texture_test_seams.hpp"
#include "src/detail/scalar_source_access.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::array<std::byte, 70> kAlphaPayloadPng{
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


void nonfinite_shader_channel_contract() {
    const auto program = compile_probe(R"(
        uniform float zero;
        half4 main(float2 p) {
            float infinity = (p.x + 1.0) / zero;
            float not_a_number = zero / zero;
            return half4(-0.25, 1.5, not_a_number, infinity);
        }
    )");
    ui::ShaderInstance shader{program};
    check(shader.set_float("zero", 0.0f) == ui::ShaderSetResult::Ok,
          "ScalarSource non-finite shader setup failed");
    const ui::Brush brush{shader};

    const auto negative = render_probe(
        scalar_probe(ui::ScalarSource::from_brush(
            brush, ui::ScalarChannel::Red)),
        8, 8);
    const auto above_one = render_probe(
        scalar_probe(ui::ScalarSource::from_brush(
            brush, ui::ScalarChannel::Green)),
        8, 8);
    const auto nan_value = render_probe(
        scalar_probe(ui::ScalarSource::from_brush(
            brush, ui::ScalarChannel::Blue)),
        8, 8);
    const auto infinity = render_probe(
        scalar_probe(ui::ScalarSource::from_brush(
            brush, ui::ScalarChannel::Alpha)),
        8, 8);

    check(negative[0] < 0.0f,
          "ScalarSource clamped a negative Shader Brush channel");
    check(above_one[0] > 1.0f,
          "ScalarSource clamped a greater-than-one Shader Brush channel");
    check(std::isnan(nan_value[0]),
          "ScalarSource did not preserve Shader Brush NaN classification");
    check(std::isinf(infinity[0]) && infinity[0] > 0.0f,
          "ScalarSource did not preserve Shader Brush infinity");
}

void source_construction_side_effect_contract() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    check(image.valid(), "ScalarSource side-effect fixture did not decode");

    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_interpretation(ui::TextureInterpretation::Data);
    const ui::Brush data_brush{texture};

    const auto decode_before =
        ui::detail::image_decode_call_count_for_test();
    const auto materialization_before =
        ui::detail::image_texture_materialization_call_count_for_test();
    const auto compile_before =
        ui::detail::shader_compile_call_count_for_test();

    const auto data_scalar =
        ui::ScalarSource::from_brush(data_brush, ui::ScalarChannel::Red);
    check(ui::detail::ScalarSourceAccess::brush(data_scalar) != nullptr,
          "ScalarSource lost ImageTexture Brush storage");
    check(ui::detail::image_decode_call_count_for_test() == decode_before,
          "from_brush decoded Image data");
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              materialization_before,
          "from_brush materialized an ImageTexture");
    check(ui::detail::shader_compile_call_count_for_test() == compile_before,
          "from_brush compiled shader source");

    const auto noise = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 28.0f, .seed = 0x2468ace0u});
    check(noise.ok(), "ScalarSource side-effect noise setup failed");
    const auto after_noise_compile =
        ui::detail::shader_compile_call_count_for_test();
    const auto noise_scalar = ui::ScalarSource::from_noise(noise.noise);
    check(ui::detail::ScalarSourceAccess::brush(noise_scalar) != nullptr,
          "ScalarSource lost NoiseSource storage");
    check(ui::detail::shader_compile_call_count_for_test() ==
              after_noise_compile,
          "from_noise compiled source");
    check(ui::detail::image_decode_call_count_for_test() == decode_before,
          "from_noise decoded Image data");
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              materialization_before,
          "from_noise materialized ImageTexture state");
}

void data_texture_channel_contract() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    check(image.valid(), "ScalarSource Data texture fixture did not decode");

    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    ui::TextureSampling nearest;
    nearest.set_filter(ui::TextureFilter::Nearest)
           .set_mipmap(ui::TextureMipmap::None);
    texture.set_sampling(nearest)
           .set_interpretation(ui::TextureInterpretation::Data);
    const ui::Brush data{texture};

    constexpr std::array<ui::ScalarChannel, 4> channels{
        ui::ScalarChannel::Red,
        ui::ScalarChannel::Green,
        ui::ScalarChannel::Blue,
        ui::ScalarChannel::Alpha,
    };
    constexpr std::array<float, 4> expected{
        200.0f / 255.0f,
        100.0f / 255.0f,
        50.0f / 255.0f,
        64.0f / 255.0f,
    };

    for (std::size_t index = 0; index < channels.size(); ++index) {
        const auto scalar =
            ui::ScalarSource::from_brush(data, channels[index]);
        const auto pixel = render_probe(scalar_probe(scalar), 8, 8);
        check(std::abs(pixel[0] - expected[index]) <= 0.005f,
              "ScalarSource changed raw Data texture channel value");
        check(std::abs(pixel[1] - expected[index]) <= 0.005f,
              "ScalarSource Data channel grayscale probe diverged");
        check(std::abs(pixel[2] - expected[index]) <= 0.005f,
              "ScalarSource Data channel grayscale probe diverged");
        check(std::abs(pixel[3] - 1.0f) <= 0.005f,
              "ScalarSource Data channel probe changed output coverage");
    }
}


void color_texture_channel_contract() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    check(image.valid(), "ScalarSource Color texture fixture did not decode");

    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    ui::TextureSampling nearest;
    nearest.set_filter(ui::TextureFilter::Nearest)
           .set_mipmap(ui::TextureMipmap::None);
    texture.set_sampling(nearest)
           .set_interpretation(ui::TextureInterpretation::Color);
    const ui::Brush color{texture};

    for (auto channel : {
             ui::ScalarChannel::Red,
             ui::ScalarChannel::Green,
             ui::ScalarChannel::Blue,
             ui::ScalarChannel::Alpha}) {
        const auto scalar =
            ui::ScalarSource::from_brush(color, channel);
        const auto actual = scalar_probe(scalar);
        const auto expected = select_channel(color, channel);
        check_pixel_near(
            render_probe(actual, 8, 8),
            render_probe(expected, 8, 8),
            0.005f);
    }
}

void snapshot_lifetime_contract() {
    const auto gradient_scalar = [] {
        ui::Brush temporary{ui::LinearGradient{
            {0.0f, 0.0f},
            {16.0f, 0.0f},
            ui::Color{0.0f, 0.0f, 0.0f, 1.0f},
            ui::Color{1.0f, 1.0f, 1.0f, 1.0f}}};
        return ui::ScalarSource::from_brush(
            std::move(temporary), ui::ScalarChannel::Red);
    }();

    const auto gradient_brush = scalar_probe(gradient_scalar);
    const auto left = render_probe(gradient_brush, 2, 8);
    const auto right = render_probe(gradient_brush, 13, 8);
    check(right[0] > left[0] + 0.35f,
          "ScalarSource did not retain the temporary gradient Brush");

    const auto solid_scalar = [] {
        return ui::ScalarSource::from_brush(
            ui::Brush{ui::Color{0.15f, 0.65f, 0.25f, 1.0f}},
            ui::ScalarChannel::Green);
    }();
    const auto solid_pixel =
        render_probe(scalar_probe(solid_scalar), 8, 8);
    check(solid_pixel[0] > 0.60f && solid_pixel[0] < 0.70f,
          "ScalarSource did not retain the temporary solid Brush");

    const auto shader_scalar = [] {
        const auto program = compile_probe(R"(
            half4 main(float2) {
                return half4(0.1, 0.3, 0.8, 1.0);
            }
        )");
        ui::ShaderInstance shader{program};
        return ui::ScalarSource::from_brush(
            ui::Brush{shader}, ui::ScalarChannel::Blue);
    }();
    const auto shader_pixel =
        render_probe(scalar_probe(shader_scalar), 8, 8);
    check(shader_pixel[0] > 0.75f && shader_pixel[0] < 0.85f,
          "ScalarSource did not retain the temporary Shader Brush");

    const auto image_scalar = [] {
        const auto image = ui::Image::decode(kAlphaPayloadPng);
        check(image.valid(), "ScalarSource lifetime Image fixture did not decode");
        ui::ImageTexture texture{
            image,
            {0.0f, 0.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 16.0f, 16.0f}};
        ui::TextureSampling nearest;
        nearest.set_filter(ui::TextureFilter::Nearest)
               .set_mipmap(ui::TextureMipmap::None);
        texture.set_sampling(nearest)
               .set_interpretation(ui::TextureInterpretation::Data);
        return ui::ScalarSource::from_brush(
            ui::Brush{texture}, ui::ScalarChannel::Red);
    }();
    const auto image_pixel =
        render_probe(scalar_probe(image_scalar), 8, 8);
    check(std::abs(image_pixel[0] - 200.0f / 255.0f) <= 0.005f,
          "ScalarSource did not retain the temporary ImageTexture Brush");

    const auto noise_scalar = [] {
        const auto created = ui::NoiseSource::create(
            ui::NoiseType::Value,
            {.feature_size = 20.0f, .seed = 0x10203040u});
        check(created.ok(), "ScalarSource lifetime NoiseSource setup failed");
        return ui::ScalarSource::from_noise(created.noise);
    }();
    const auto noise_pixel =
        render_probe(scalar_probe(noise_scalar), 7, 9);
    check(std::isfinite(noise_pixel[0]) &&
              noise_pixel[0] >= 0.0f && noise_pixel[0] <= 1.0f,
          "ScalarSource did not retain the temporary NoiseSource");
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


void backend_failure_retry_contract() {
    const auto program = compile_probe(R"(
        half4 main(float2) {
            return half4(0.8, 0.2, 0.1, 1.0);
        }
    )");
    ui::ShaderInstance shader{program};
    const auto scalar = ui::ScalarSource::from_brush(
        ui::Brush{shader}, ui::ScalarChannel::Red);
    const auto probe = scalar_probe(scalar);

    const auto before =
        ui::detail::shader_materialization_call_count_for_test();
    ui::detail::set_shader_materialization_failure_for_test(
        ui::detail::ShaderMaterializationFailurePoint::BeforeUniformData);

    bool failed = false;
    try {
        (void)render_probe(probe, 8, 8);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    check(failed,
          "ScalarSource hid backend materialization failure as scalar zero");
    check(ui::detail::shader_materialization_call_count_for_test() > before,
          "ScalarSource failure seam did not reach backend materialization");

    const auto recovered = render_probe(probe, 8, 8);
    check(recovered[0] > 0.75f && recovered[0] < 0.85f,
          "ScalarSource did not recover after backend materialization failure");
}

void two_renderer_isolation_contract() {
    const auto first = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.8f, 0.1f, 0.1f, 1.0f}},
        ui::ScalarChannel::Red);
    const auto second = ui::ScalarSource::from_brush(
        ui::Brush{ui::Color{0.1f, 0.7f, 0.1f, 1.0f}},
        ui::ScalarChannel::Green);
    const auto first_brush = scalar_probe(first);
    const auto second_brush = scalar_probe(second);

    auto first_tree = ui::UI{ui::Canvas{
        16.0f, 16.0f,
        [first_brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 16.0f, 16.0f}, first_brush);
        }}};
    auto second_tree = ui::UI{ui::Canvas{
        16.0f, 16.0f,
        [second_brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 16.0f, 16.0f}, second_brush);
        }}};

    ui::HeadlessRenderer second_renderer{{16.0f, 16.0f}, 1.0f};
    {
        ui::HeadlessRenderer first_renderer{{16.0f, 16.0f}, 1.0f};
        check(first_renderer.render(first_tree),
              "first ScalarSource renderer failed");
        const auto first_pixel = first_renderer.pixel(8, 8);
        check(first_pixel.r > 150,
              "first ScalarSource renderer produced the wrong channel");
    }

    check(second_renderer.render(second_tree),
          "second ScalarSource renderer failed after first destruction");
    const auto before = second_renderer.rgba_pixels();
    check(second_renderer.render(second_tree),
          "second ScalarSource renderer failed on repeat");
    check(second_renderer.rgba_pixels() == before,
          "second ScalarSource renderer changed after first destruction");
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
    nonfinite_shader_channel_contract();
    source_construction_side_effect_contract();
    data_texture_channel_contract();
    color_texture_channel_contract();
    snapshot_lifetime_contract();
    noise_sampling_contract();
    backend_failure_retry_contract();
    independent_source_contract();
    two_renderer_isolation_contract();
    return 0;
}
