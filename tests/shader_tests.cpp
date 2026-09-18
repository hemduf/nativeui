#include "test_support.hpp"

#include <nativeui/shader.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

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
    const auto success = [] {
        std::string source{kValidShader};
        return ui::ShaderProgram::compile(source);
    }();
    NUI_CHECK(success.ok());
    NUI_CHECK(success.program);
    NUI_CHECK(success.diagnostics.empty());

    const auto failure = [] {
        std::string source{"half4 main(float2 p) { return nope; }"};
        return ui::ShaderProgram::compile(source);
    }();
    check_compile_failure(failure);
    NUI_CHECK(!failure.diagnostics.front().message.empty());
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
    repeated_failed_compile_remains_usable();
}

} // namespace

int main() { return test::run("shader", &suite); }
