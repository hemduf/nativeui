#include <nativeui/shader.hpp>

#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>

namespace ui {
ShaderCompileResult compile_shader_program_for_test(std::string_view sksl,
                                                    int injected_failure);
} // namespace ui

namespace {

constexpr int kFailBeforeDiagnosticOwnership = 1;
constexpr int kFailProgramDataAllocation = 2;
constexpr int kFailProgramPublicationAllocation = 3;
constexpr int kForceEmptyBackendDiagnostic = 4;

constexpr std::string_view kValidShader = R"(
    half4 main(float2 p) {
        return half4(0.25, 0.5, 0.75, 1.0);
    }
)";

void check(bool condition, const char* message) {
    if (!condition) throw message;
}

void check_compile_failure(const ui::ShaderCompileResult& result) {
    check(!result.ok(), "fault result unexpectedly succeeded");
    check(!result.program, "fault result published a program");
    check(!result.diagnostics.empty(), "fault result has no diagnostic");
    for (const auto& diagnostic : result.diagnostics) {
        check(diagnostic.code == ui::ShaderCompileError::CompileError,
              "fault diagnostic classification mismatch");
        check(diagnostic.line == 0 && diagnostic.column == 0,
              "fault diagnostic source location mismatch");
        check(!diagnostic.message.empty(), "fault diagnostic message is empty");
    }
}

void expect_bad_alloc(std::string_view source, int failure_point) {
    bool threw = false;
    try {
        (void)ui::compile_shader_program_for_test(source, failure_point);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "fault seam did not propagate bad_alloc");

    const auto later = ui::ShaderProgram::compile(kValidShader);
    check(later.ok() && later.program && later.diagnostics.empty(),
          "compile did not recover after injected allocation failure");
}

void suite() {
    expect_bad_alloc(
        "half4 main(float2 p) { return missing_symbol; }",
        kFailBeforeDiagnosticOwnership);
    expect_bad_alloc(kValidShader, kFailProgramDataAllocation);
    expect_bad_alloc(kValidShader, kFailProgramPublicationAllocation);

    const auto fallback = ui::compile_shader_program_for_test(
        "half4 main(float2 p) { return missing_symbol; }",
        kForceEmptyBackendDiagnostic);
    check_compile_failure(fallback);
    check(fallback.diagnostics.size() == 1U,
          "fallback diagnostic count mismatch");
    check(fallback.diagnostics.front().message ==
              "SkSL runtime-shader compilation failed",
          "fallback diagnostic text mismatch");
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS shader_faults\n";
        return EXIT_SUCCESS;
    } catch (const char* error) {
        std::cerr << "FAIL shader_faults: " << error << '\n';
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "FAIL shader_faults: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
