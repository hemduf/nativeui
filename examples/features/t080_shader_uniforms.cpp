#include "example_support.hpp"

#include <nativeui/shader.hpp>

#include <array>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kShader = R"(
    uniform float gain;
    uniform float3 lightDir;
    layout(color) uniform half4 baseColor;

    half4 main(float2 p) {
        return half4(baseColor.rgb * gain, baseColor.a);
    }
)";

int self_test() {
    const auto compiled = ui::ShaderProgram::compile(kShader);
    if (!compiled.ok() || !compiled.program || !compiled.diagnostics.empty()) {
        return example::fail("typed shader did not compile");
    }

    const auto uniforms = compiled.program->uniforms();
    if (uniforms.size() != 3U ||
        uniforms[0].name != "gain" ||
        uniforms[0].type != ui::ShaderUniformType::Float ||
        uniforms[1].name != "lightDir" ||
        uniforms[1].type != ui::ShaderUniformType::Float3 ||
        uniforms[2].name != "baseColor" ||
        uniforms[2].type != ui::ShaderUniformType::Color) {
        return example::fail("uniform reflection contract mismatch");
    }

    ui::ShaderInstance shader{compiled.program};
    if (!shader.valid() ||
        shader.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok ||
        shader.set_float3("lightDir", {-0.4f, -0.5f, 0.75f}) !=
            ui::ShaderSetResult::Ok ||
        shader.set_color("baseColor", {0.65f, 0.67f, 0.70f, 1.0f}) !=
            ui::ShaderSetResult::Ok) {
        return example::fail("typed uniform binding failed");
    }

    ui::ShaderInstance independent{shader};
    if (shader.set_float("gain", 0.25f) != ui::ShaderSetResult::Ok ||
        independent.set_float("gain", 1.0f) != ui::ShaderSetResult::Ok) {
        return example::fail("copy isolation setup failed");
    }

    ui::ShaderInstance moved{std::move(shader)};
    if (!moved.valid() || shader.valid() ||
        shader.set_float("gain", 1.0f) != ui::ShaderSetResult::NotFound) {
        return example::fail("move/inert contract mismatch");
    }

    shader = independent;
    if (!shader.valid() ||
        shader.set_float("gain", 0.5f) != ui::ShaderSetResult::Ok) {
        return example::fail("inert recovery by assignment failed");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        return self_test();
    }

    const auto compiled = ui::ShaderProgram::compile(kShader);
    if (!compiled.ok()) {
        for (const auto& diagnostic : compiled.diagnostics) {
            std::cerr << "SkSL compile/profile error: " << diagnostic.message << '\n';
        }
        return 1;
    }

    ui::ShaderInstance shader{compiled.program};
    shader.set_float("gain", 0.75f);
    shader.set_float3("lightDir", {-0.4f, -0.5f, 0.75f});
    shader.set_color("baseColor", {0.65f, 0.67f, 0.70f, 1.0f});

    std::cout
        << "T080: reflected " << compiled.program->uniforms().size()
        << " typed SkSL uniforms and prepared one independent ShaderInstance.\n"
        << "Brush/render materialization is intentionally deferred to T081.\n";
    return 0;
}
