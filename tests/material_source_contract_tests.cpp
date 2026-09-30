#include <nativeui/image.hpp>
#include <nativeui/material.hpp>
#include <nativeui/noise.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/image_texture_brush_access.hpp"
#include "src/detail/image_texture_test_seams.hpp"
#include "src/detail/scalar_source_access.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>

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
    clear_releases_emissive_resource_ownership();
    return 0;
}
