#include "test_support.hpp"

#include <nativeui/shader.hpp>

#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>

namespace ui::detail {
ShaderCompileResult compile_shader_program_for_test(std::string_view sksl,
                                                    int injected_failure);
} // namespace ui::detail

namespace {

constexpr int kFailBeforeDiagnosticOwnership = 1;
constexpr int kFailBeforeProgramData = 2;
constexpr int kFailProgramPublicationAllocation = 3;
constexpr int kForceEmptyBackendDiagnostic = 4;

constexpr std::string_view kValidShader = R"(
    half4 main(float2 p) {
        return half4(0.25, 0.5, 0.75, 1.0);
    }
)";

void api_traits() {
    static_assert(!std::is_default_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_copy_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_copy_assignable_v<ui::ShaderProgram>);
    static_assert(!std::is_move_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_move_assignable_v<ui::ShaderProgram>);
    static_assert(std::is_nothrow_destructible_v<ui::ShaderProgram>);
}

void check_compile_failure(const ui::ShaderCompileResult& result) {
    NUI_CHECK(!result.ok());
    NUI_CHECK(!result.program);
    NUI_CHECK(!result.diagnostics.empty());
    for (const auto& diagnostic : result.diagnostics) {
        NUI_CHECK(diagnostic.code != ui::ShaderCompileError::None);
        NUI_CHECK(diagnostic.code == ui::ShaderCompileError::CompileError);
        NUI_CHECK(diagnostic.line == 0);
        NUI_CHECK(diagnostic.column == 0);
        NUI_CHECK(!diagnostic.message.empty());
    }
}

void exact_result_contract() {
    const auto valid = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(valid.ok());
    NUI_CHECK(valid.program);
    NUI_CHECK(valid.diagnostics.empty());

    check_compile_failure(ui::ShaderProgram::compile(""));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float2 p) {
            return half4(1.0, 0.0, 0.0, 1.0)
        }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float2 p) {
            return missing_symbol;
        }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float4 p) {
            return half4(1.0);
        }
    )"));
}

void caller_source_lifetime_is_independent() {
    std::shared_ptr<const ui::ShaderProgram> program;
    {
        std::string source{kValidShader};
        const auto result = ui::ShaderProgram::compile(source);
        NUI_CHECK(result.ok());
        program = result.program;
        source.assign("destroyed");
        source.shrink_to_fit();
    }
    NUI_CHECK(program);

    std::string diagnostic_message;
    {
        std::string source{"half4 main(float2 p) { return nope; }"};
        const auto result = ui::ShaderProgram::compile(source);
        check_compile_failure(result);
        diagnostic_message = result.diagnostics.front().message;
        source.clear();
        source.shrink_to_fit();
    }
    NUI_CHECK(!diagnostic_message.empty());
}

void immutable_program_can_be_retained_by_two_uis() {
    auto result = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(result.ok());
    auto shared = std::move(result.program);
    std::weak_ptr<const ui::ShaderProgram> lifetime = shared;

    const auto make_ui = [](std::shared_ptr<const ui::ShaderProgram> program) {
        return std::make_unique<ui::UI>(
            ui::Canvas{2.0f, 2.0f, [program = std::move(program)](ui::CanvasContext2D& canvas) {
                NUI_CHECK(program);
                canvas.fill_rect(
                    {0.0f, 0.0f, 2.0f, 2.0f},
                    ui::Color{0.0f, 0.0f, 0.0f, 1.0f});
            }});
    };

    auto first = make_ui(shared);
    auto second = make_ui(shared);
    shared.reset();

    ui::HeadlessRenderer renderer{{2.0f, 2.0f}, 1.0f};
    NUI_CHECK(renderer.render(*first));
    first.reset();
    NUI_CHECK(!lifetime.expired());

    NUI_CHECK(renderer.render(*second));
    second.reset();
    NUI_CHECK(lifetime.expired());
}

void expect_injected_bad_alloc(std::string_view source, int failure_point) {
    bool threw = false;
    try {
        (void)ui::detail::compile_shader_program_for_test(source, failure_point);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);

    const auto later = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(later.ok());
    NUI_CHECK(later.program);
    NUI_CHECK(later.diagnostics.empty());
}

void deterministic_failure_injection_has_strong_recovery() {
    expect_injected_bad_alloc(
        "half4 main(float2 p) { return missing_symbol; }",
        kFailBeforeDiagnosticOwnership);
    expect_injected_bad_alloc(kValidShader, kFailBeforeProgramData);
    expect_injected_bad_alloc(kValidShader, kFailProgramPublicationAllocation);
}

void empty_backend_diagnostic_uses_nativeui_fallback() {
    const auto result = ui::detail::compile_shader_program_for_test(
        "half4 main(float2 p) { return missing_symbol; }",
        kForceEmptyBackendDiagnostic);
    check_compile_failure(result);
    NUI_CHECK(result.diagnostics.size() == 1U);
    NUI_CHECK(result.diagnostics.front().message ==
              "SkSL runtime-shader compilation failed");
}

void repeated_failed_compile_remains_usable() {
    for (int i = 0; i < 128; ++i) {
        check_compile_failure(
            ui::ShaderProgram::compile("half4 main(float2 p) { return nope; }"));
    }

    const auto valid = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(valid.ok());
}

void suite() {
    api_traits();
    exact_result_contract();
    caller_source_lifetime_is_independent();
    immutable_program_can_be_retained_by_two_uis();
    deterministic_failure_injection_has_strong_recovery();
    empty_backend_diagnostic_uses_nativeui_fallback();
    repeated_failed_compile_remains_usable();
}

} // namespace

int main() { return test::run("shader", &suite); }
