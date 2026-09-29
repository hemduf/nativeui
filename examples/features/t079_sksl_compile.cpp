#include "example_support.hpp"

#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kValidShader = R"(
    half4 main(float2 p) {
        float v = sin(p.x * 0.08) * 0.5 + 0.5;
        return half4(v, v, v, 1.0);
    }
)";

int self_test() {
    const auto valid = ui::ShaderProgram::compile(kValidShader);
    if (!valid.ok() || !valid.program || !valid.diagnostics.empty()) {
        return example::fail("valid SkSL did not produce exactly one successful program result");
    }

    const auto invalid =
        ui::ShaderProgram::compile("half4 main(float2 p) { return missing_symbol; }");
    if (invalid.ok() || invalid.program || invalid.diagnostics.empty()) {
        return example::fail("invalid SkSL did not produce a compile diagnostic");
    }
    for (const auto& diagnostic : invalid.diagnostics) {
        if (diagnostic.code != ui::ShaderCompileError::CompileError ||
            diagnostic.line != 0 ||
            diagnostic.column != 0 ||
            diagnostic.message.empty()) {
            return example::fail("compile diagnostic contract mismatch");
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        return self_test();
    }

    const auto compiled = ui::ShaderProgram::compile(kValidShader);
    if (!compiled.ok()) {
        for (const auto& diagnostic : compiled.diagnostics) {
            std::cerr << "SkSL compile error: " << diagnostic.message << '\n';
        }
        return 1;
    }

    std::cout
        << "T079: SkSL compiled explicitly into an immutable NativeUI ShaderProgram.\n"
        << "Rendering, uniform binding and child shaders are intentionally outside this example.\n";
    return 0;
}
