#include <nativeui/noise.hpp>
#include <nativeui/scalar_source.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/scalar_source_access.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include <stdexcept>

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

} // namespace

int main() {
    source_creation_contract();
    return 0;
}
