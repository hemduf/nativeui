#include <nativeui/shader.hpp>

#include "include/core/SkString.h"
#include "include/effects/SkRuntimeEffect.h"

#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

struct ShaderProgramData final {
    explicit ShaderProgramData(sk_sp<SkRuntimeEffect> effect_in) noexcept
        : effect(std::move(effect_in)) {}

    sk_sp<SkRuntimeEffect> effect;
};

namespace {

enum class CompileFailurePoint : int {
    None = 0,
    BeforeDiagnosticOwnership = 1,
    BeforeProgramData = 2,
    BeforeProgramPublication = 3,
};

[[nodiscard]] CompileFailurePoint decode_failure_point(int value) noexcept {
    switch (value) {
        case 1: return CompileFailurePoint::BeforeDiagnosticOwnership;
        case 2: return CompileFailurePoint::BeforeProgramData;
        case 3: return CompileFailurePoint::BeforeProgramPublication;
        default: return CompileFailurePoint::None;
    }
}

[[nodiscard]] ShaderDiagnostic compiler_diagnostic(const SkString& error_text) {
    std::string message{error_text.data(), error_text.size()};
    if (message.empty()) {
        message = "SkSL runtime-shader compilation failed";
    }
    return ShaderDiagnostic{
        ShaderCompileError::CompileError,
        0,
        0,
        std::move(message),
    };
}

} // namespace

struct ShaderProgramCompiler {
    [[nodiscard]] static ShaderCompileResult compile(std::string_view sksl,
                                                     int injected_failure = 0) {
        const auto failure = decode_failure_point(injected_failure);
        auto backend = SkRuntimeEffect::MakeForShader(SkString{sksl});

        if (!backend.effect) {
            if (failure == CompileFailurePoint::BeforeDiagnosticOwnership) {
                throw std::bad_alloc{};
            }

            std::vector<ShaderDiagnostic> diagnostics;
            diagnostics.reserve(1);
            diagnostics.push_back(compiler_diagnostic(backend.errorText));
            return ShaderCompileResult{nullptr, std::move(diagnostics)};
        }

        if (failure == CompileFailurePoint::BeforeProgramData) {
            throw std::bad_alloc{};
        }

        auto mutable_data =
            std::make_shared<ShaderProgramData>(std::move(backend.effect));
        std::shared_ptr<const ShaderProgramData> data = std::move(mutable_data);

        auto candidate =
            std::unique_ptr<ShaderProgram>{new ShaderProgram{std::move(data)}};

        if (failure == CompileFailurePoint::BeforeProgramPublication) {
            throw std::bad_alloc{};
        }

        std::shared_ptr<const ShaderProgram> program{std::move(candidate)};
        return ShaderCompileResult{std::move(program), {}};
    }
};

// Test-only private seam. It carries failure selection as an argument so the
// implementation needs no mutable global/thread-local injection state.
ShaderCompileResult compile_shader_program_for_test(std::string_view sksl,
                                                    int injected_failure) {
    return ShaderProgramCompiler::compile(sksl, injected_failure);
}

} // namespace ui::detail

namespace ui {

ShaderProgram::ShaderProgram(
    std::shared_ptr<const detail::ShaderProgramData> data) noexcept
    : data_(std::move(data)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

ShaderCompileResult ShaderProgram::compile(std::string_view sksl) {
    return detail::ShaderProgramCompiler::compile(sksl);
}

} // namespace ui
