#include <nativeui/image.hpp>
#include <nativeui/material.hpp>
#include <nativeui/noise.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/image_texture_brush_access.hpp"
#include "src/detail/image_texture_test_seams.hpp"
#include "src/detail/material_access.hpp"
#include "src/detail/scalar_source_access.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <algorithm>
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

std::shared_ptr<const ui::ShaderProgram> compile_probe(std::string_view source) {
    const auto compiled = ui::ShaderProgram::compile(source);
    check(compiled.ok() && compiled.program != nullptr,
          "Material probe shader compilation failed");
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
    throw std::runtime_error{"invalid Material probe channel"};
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
          "Material probe child binding failed");
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
              "Material constant probe binding failed");
        return ui::Brush{probe};
    }

    const auto* brush = ui::detail::ScalarSourceAccess::brush(source);
    check(brush != nullptr, "Material probe lost ScalarSource Brush storage");
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
    check(surface != nullptr, "Material probe surface creation failed");
    auto* canvas = surface->getCanvas();
    check(canvas != nullptr, "Material probe canvas missing");
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
    check(surface->peekPixels(&pixmap), "Material probe pixels missing");
    const auto value = pixmap.getColor4f(x, y);
    return {value.fR, value.fG, value.fB, value.fA};
}

void check_pixel_near(const std::array<float, 4>& actual,
                      const std::array<float, 4>& expected,
                      float tolerance = 0.0025f) {
    for (std::size_t index = 0; index < actual.size(); ++index) {
        check(std::abs(actual[index] - expected[index]) <= tolerance,
              "Material probe pixel mismatch");
    }
}

void prepared_setters_do_no_source_or_backend_work() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    check(image.valid(), "Material source fixture image decode failed");

    ui::ImageTexture texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    texture.set_interpretation(ui::TextureInterpretation::Data);
    const ui::Brush image_brush{texture};
    const auto image_scalar =
        ui::ScalarSource::from_brush(image_brush, ui::ScalarChannel::Red);

    const auto compiled = ui::ShaderProgram::compile(R"(
        half4 main(float2) { return half4(0.25, 0.5, 0.75, 1.0); }
    )");
    check(compiled.ok(), "Material source fixture shader compile failed");
    ui::ShaderInstance shader{compiled.program};
    const ui::Brush shader_brush{shader};

    const auto noise = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 28.0f, .seed = 0x2468ace0u});
    check(noise.ok(), "Material source fixture noise creation failed");
    const auto noise_scalar = ui::ScalarSource::from_noise(noise.noise);

    const auto compile_before =
        ui::detail::shader_compile_call_count_for_test();
    const auto shader_materialization_before =
        ui::detail::shader_materialization_call_count_for_test();
    const auto decode_before =
        ui::detail::image_decode_call_count_for_test();
    const auto image_materialization_before =
        ui::detail::image_texture_materialization_call_count_for_test();

    ui::Material material;
    material.set_albedo(shader_brush)
            .set_roughness(image_scalar)
            .set_metallic(noise_scalar)
            .set_emissive(image_brush, noise_scalar);
    material.clear_emissive();

    check(ui::detail::shader_compile_call_count_for_test() == compile_before,
          "Material prepared setters compiled source");
    check(ui::detail::shader_materialization_call_count_for_test() ==
              shader_materialization_before,
          "Material prepared setters materialized shader backend state");
    check(ui::detail::image_decode_call_count_for_test() == decode_before,
          "Material prepared setters decoded image data");
    check(ui::detail::image_texture_materialization_call_count_for_test() ==
              image_materialization_before,
          "Material prepared setters materialized ImageTexture backend state");

    const auto* stored_brush =
        ui::detail::ScalarSourceAccess::brush(material.roughness());
    check(stored_brush != nullptr,
          "Material lost prepared ImageTexture ScalarSource storage");
    const auto* stored_texture =
        ui::detail::ImageTextureBrushAccess::texture(*stored_brush);
    check(stored_texture != nullptr,
          "Material changed ImageTexture ScalarSource representation");
    check(stored_texture->interpretation() == ui::TextureInterpretation::Data,
          "Material changed Data texture interpretation");
    check(ui::detail::ScalarSourceAccess::channel(material.roughness()) ==
              ui::ScalarChannel::Red,
          "Material changed ScalarSource channel");
}

void shader_snapshot_is_owned_independently() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float level;
        half4 main(float2) { return half4(level, level, level, 1.0); }
    )");
    check(compiled.ok(), "Material snapshot fixture shader compile failed");

    ui::ShaderInstance shader{compiled.program};
    check(shader.set_float("level", 0.25f) == ui::ShaderSetResult::Ok,
          "Material snapshot fixture uniform setup failed");
    const ui::Brush original_snapshot{shader};

    ui::Material material{original_snapshot};

    check(shader.set_float("level", 0.75f) == ui::ShaderSetResult::Ok,
          "Material snapshot fixture uniform mutation failed");
    const ui::Brush mutated_snapshot{shader};

    const auto stored =
        ui::detail::ShaderBrushAccess::binding_bytes(material.albedo());
    const auto original =
        ui::detail::ShaderBrushAccess::binding_bytes(original_snapshot);
    const auto mutated =
        ui::detail::ShaderBrushAccess::binding_bytes(mutated_snapshot);

    check(stored.size() == original.size(),
          "Material changed Shader Brush binding shape");
    check(std::equal(stored.begin(), stored.end(), original.begin()),
          "Material did not retain the original Shader Brush snapshot");
    check(stored.size() == mutated.size(),
          "Material mutated Shader Brush binding shape");
    check(!std::equal(stored.begin(), stored.end(), mutated.begin()),
          "Material observed later caller-side ShaderInstance mutation");
}

void check_rect(ui::Rect actual, ui::Rect expected) {
    check(actual.x == expected.x &&
              actual.y == expected.y &&
              actual.w == expected.w &&
              actual.h == expected.h,
          "Material changed ImageTexture mapping rectangle");
}

void check_transform(ui::Transform2D actual, ui::Transform2D expected) {
    check(actual.m00 == expected.m00 &&
              actual.m01 == expected.m01 &&
              actual.m02 == expected.m02 &&
              actual.m10 == expected.m10 &&
              actual.m11 == expected.m11 &&
              actual.m12 == expected.m12,
          "Material introduced a second source transform");
}

void nested_source_mapping_and_lifetime_contract() {
    ui::Material material;
    const ui::Rect albedo_destination{2.0f, 3.0f, 18.0f, 20.0f};
    const ui::Rect roughness_destination{5.0f, 7.0f, 11.0f, 13.0f};
    const auto albedo_transform =
        ui::Transform2D::translation(3.0f, 4.0f) *
        ui::Transform2D::scaling(0.5f, 0.75f);
    const auto roughness_transform =
        ui::Transform2D::translation(-2.0f, 6.0f);

    {
        const auto image = ui::Image::decode(kAlphaPayloadPng);
        check(image.valid(), "Material mapping fixture image decode failed");

        ui::ImageTexture albedo_texture{
            image,
            {0.0f, 0.0f, 1.0f, 1.0f},
            albedo_destination};
        albedo_texture
            .set_interpretation(ui::TextureInterpretation::Color)
            .set_transform(albedo_transform);

        ui::ImageTexture roughness_texture{
            image,
            {0.0f, 0.0f, 1.0f, 1.0f},
            roughness_destination};
        roughness_texture
            .set_interpretation(ui::TextureInterpretation::Data)
            .set_transform(roughness_transform);

        const auto noise = ui::NoiseSource::create(
            ui::NoiseType::Value,
            {.feature_size = 20.0f, .seed = 0x10203040u});
        check(noise.ok(), "Material mapping fixture noise creation failed");

        material
            .set_albedo(ui::Brush{albedo_texture})
            .set_roughness(ui::ScalarSource::from_brush(
                ui::Brush{roughness_texture},
                ui::ScalarChannel::Green))
            .set_metallic(ui::ScalarSource::from_noise(noise.noise));
    }

    const auto* stored_albedo =
        ui::detail::ImageTextureBrushAccess::texture(material.albedo());
    check(stored_albedo != nullptr && stored_albedo->valid(),
          "Material did not retain ImageTexture albedo ownership");
    check(stored_albedo->interpretation() == ui::TextureInterpretation::Color,
          "Material changed Color texture interpretation");
    check_rect(stored_albedo->destination(), albedo_destination);
    check_transform(stored_albedo->transform(), albedo_transform);

    const auto* roughness_brush =
        ui::detail::ScalarSourceAccess::brush(material.roughness());
    check(roughness_brush != nullptr,
          "Material did not retain Brush-backed roughness ownership");
    const auto* stored_roughness =
        ui::detail::ImageTextureBrushAccess::texture(*roughness_brush);
    check(stored_roughness != nullptr && stored_roughness->valid(),
          "Material lost roughness ImageTexture ownership");
    check(stored_roughness->interpretation() ==
              ui::TextureInterpretation::Data,
          "Material changed Data texture interpretation");
    check(ui::detail::ScalarSourceAccess::channel(material.roughness()) ==
              ui::ScalarChannel::Green,
          "Material changed roughness source channel");
    check_rect(stored_roughness->destination(), roughness_destination);
    check_transform(stored_roughness->transform(), roughness_transform);

    const auto* metallic_brush =
        ui::detail::ScalarSourceAccess::brush(material.metallic());
    check(metallic_brush != nullptr &&
              ui::detail::ShaderBrushAccess::is_shader(*metallic_brush),
          "Material did not retain NoiseSource-backed metallic ownership");
}

void shared_painter_coordinate_and_post_sample_sanitation_contract() {
    const auto program = compile_probe(R"(
        half4 main(float2 p) {
            return half4(p.x / 16.0, p.y / 16.0, p.x / 4.0 - 1.0, 1.0);
        }
    )");
    ui::ShaderInstance shader{program};
    const ui::Brush coordinate_source{shader};

    ui::Material material{coordinate_source};
    material
        .set_roughness(ui::ScalarSource::from_brush(
            coordinate_source, ui::ScalarChannel::Blue))
        .set_metallic(ui::ScalarSource::from_brush(
            coordinate_source, ui::ScalarChannel::Red))
        .set_emissive(
            coordinate_source,
            ui::ScalarSource::from_brush(
                coordinate_source, ui::ScalarChannel::Green));

    const auto roughness = scalar_probe(material.roughness());
    const auto metallic = scalar_probe(material.metallic());
    const auto emissive_intensity = scalar_probe(material.emissive_intensity());

    check_pixel_near(
        render_probe(roughness, 3, 7),
        render_probe(
            select_channel(material.albedo(), ui::ScalarChannel::Blue),
            3, 7));
    check_pixel_near(
        render_probe(metallic, 12, 7),
        render_probe(
            select_channel(material.albedo(), ui::ScalarChannel::Red),
            12, 7));
    check_pixel_near(
        render_probe(emissive_intensity, 7, 7, 2.0f),
        render_probe(
            select_channel(material.emissive_color(), ui::ScalarChannel::Green),
            7, 7, 2.0f));

    const auto low = render_probe(roughness, 0, 7);
    const auto high = render_probe(roughness, 12, 7);
    check(low[0] < 0.0f,
          "Material pre-clamped a negative sampled roughness source");
    check(high[0] > 1.0f,
          "Material pre-clamped an above-one sampled roughness source");
    check(ui::detail::MaterialAccess::sanitize_roughness(low[0]) == 0.045f,
          "Material roughness sanitation did not occur after sampling");
    check(ui::detail::MaterialAccess::sanitize_roughness(high[0]) == 1.0f,
          "Material roughness upper sanitation did not occur after sampling");
}

void color_and_data_source_semantics_survive_material_storage() {
    const auto image = ui::Image::decode(kAlphaPayloadPng);
    check(image.valid(), "Material Color/Data fixture image decode failed");

    ui::ImageTexture color_texture{
        image,
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 16.0f, 16.0f}};
    color_texture.set_interpretation(ui::TextureInterpretation::Color);
    auto data_texture = color_texture;
    data_texture.set_interpretation(ui::TextureInterpretation::Data);

    const ui::Brush color_brush{color_texture};
    const ui::Brush data_brush{data_texture};
    const auto data_scalar = ui::ScalarSource::from_brush(
        data_brush, ui::ScalarChannel::Green);

    ui::Material material{color_brush};
    material.set_roughness(data_scalar);

    check_pixel_near(
        render_probe(material.albedo(), 8, 8),
        render_probe(color_brush, 8, 8));
    check_pixel_near(
        render_probe(scalar_probe(material.roughness()), 8, 8),
        render_probe(scalar_probe(data_scalar), 8, 8));

    const auto* stored_color =
        ui::detail::ImageTextureBrushAccess::texture(material.albedo());
    check(stored_color != nullptr &&
              stored_color->interpretation() == ui::TextureInterpretation::Color,
          "Material changed Color texture interpretation");

    const auto* stored_data_brush =
        ui::detail::ScalarSourceAccess::brush(material.roughness());
    check(stored_data_brush != nullptr,
          "Material lost Data-backed ScalarSource storage");
    const auto* stored_data =
        ui::detail::ImageTextureBrushAccess::texture(*stored_data_brush);
    check(stored_data != nullptr &&
              stored_data->interpretation() == ui::TextureInterpretation::Data,
          "Material changed Data texture interpretation");
    check(ui::detail::ScalarSourceAccess::channel(material.roughness()) ==
              ui::ScalarChannel::Green,
          "Material changed Data ScalarSource channel");
}

void clear_releases_emissive_resource_ownership() {
    std::weak_ptr<const ui::ShaderProgram> program_lifetime;
    ui::Material original;
    ui::Material copy;

    {
        auto compiled = ui::ShaderProgram::compile(R"(
            half4 main(float2) { return half4(1.0, 0.25, 0.1, 1.0); }
        )");
        check(compiled.ok(), "Material lifetime fixture shader compile failed");
        program_lifetime = compiled.program;

        {
            ui::ShaderInstance shader{compiled.program};
            original.set_emissive(ui::Brush{shader}, 2.0f);
            copy = original;
        }

        compiled.program.reset();
    }

    original.clear_emissive();
    check(!program_lifetime.expired(),
          "Material copy did not retain emissive shader ownership");

    copy.clear_emissive();
    check(program_lifetime.expired(),
          "Material clear_emissive retained stale shader ownership");
}

} // namespace

int main() {
    prepared_setters_do_no_source_or_backend_work();
    shader_snapshot_is_owned_independently();
    nested_source_mapping_and_lifetime_contract();
    shared_painter_coordinate_and_post_sample_sanitation_contract();
    color_and_data_source_semantics_survive_material_storage();
    clear_releases_emissive_resource_ownership();
    return 0;
}
