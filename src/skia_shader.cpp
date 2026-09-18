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
    ProgramDataAllocation = 2,
    ProgramPublicationAllocation = 3,
    EmptyBackendDiagnostic = 4,
};

[[nodiscard]] CompileFailurePoint decode_failure_point(int value) noexcept {
    switch (value) {
        case 1: return CompileFailurePoint::BeforeDiagnosticOwnership;
        case 2: return CompileFailurePoint::ProgramDataAllocation;
        case 3: return CompileFailurePoint::ProgramPublicationAllocation;
        case 4: return CompileFailurePoint::EmptyBackendDiagnostic;
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

template <class T>
class PublicationAllocator {
public:
    using value_type = T;

    explicit PublicationAllocator(bool fail_allocation = false) noexcept
        : fail_allocation_(fail_allocation) {}

    template <class U>
    PublicationAllocator(const PublicationAllocator<U>& other) noexcept
        : fail_allocation_(other.fail_allocation_) {}

    [[nodiscard]] T* allocate(std::size_t count) {
        if (fail_allocation_) throw std::bad_alloc{};
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template <class U>
    [[nodiscard]] bool operator==(const PublicationAllocator<U>& other) const noexcept {
        return fail_allocation_ == other.fail_allocation_;
    }

private:
    template <class>
    friend class PublicationAllocator;

    bool fail_allocation_{};
};

[[nodiscard]] std::shared_ptr<const ShaderProgram> publish_program(
    std::unique_ptr<ShaderProgram> candidate,
    bool fail_control_block_allocation) {
    auto* raw = candidate.release();

    // shared_ptr(Y*, D, A) is required to invoke D(raw) if control-block
    // allocation throws, so this exercises the real publication allocation
    // while preserving the strong no-partial-publication guarantee.
    return std::shared_ptr<const ShaderProgram>{
        raw,
        std::default_delete<ShaderProgram>{},
        PublicationAllocator<ShaderProgram>{fail_control_block_allocation},
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
            if (failure == CompileFailurePoint::EmptyBackendDiagnostic) {
                backend.errorText.reset();
            }

            std::vector<ShaderDiagnostic> diagnostics;
            diagnostics.reserve(1);
            diagnostics.push_back(compiler_diagnostic(backend.errorText));
            return ShaderCompileResult{nullptr, std::move(diagnostics)};
        }

        auto mutable_data = std::allocate_shared<ShaderProgramData>(
            PublicationAllocator<ShaderProgramData>{
                failure == CompileFailurePoint::ProgramDataAllocation},
            std::move(backend.effect));
        std::shared_ptr<const ShaderProgramData> data = std::move(mutable_data);

        auto candidate =
            std::unique_ptr<ShaderProgram>{new ShaderProgram{std::move(data)}};
        auto program = publish_program(
            std::move(candidate),
            failure == CompileFailurePoint::ProgramPublicationAllocation);
        return ShaderCompileResult{std::move(program), {}};
    }
};

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
// Test-only private seam. It carries failure selection as an argument so the
// implementation needs no mutable global/thread-local injection state and is
// absent from normal release builds.
ShaderCompileResult compile_shader_program_for_test(std::string_view sksl,
                                                    int injected_failure) {
    return ShaderProgramCompiler::compile(sksl, injected_failure);
}
#endif

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
