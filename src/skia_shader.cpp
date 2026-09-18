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

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class CompileFailurePoint {
    None,
    BeforeDiagnosticOwnership,
    AfterDiagnosticOwnership,
    ProgramDataAllocation,
    BeforeProgramWrapperAllocation,
    ProgramPublicationAllocation,
    EmptyBackendDiagnostic,
};

[[nodiscard]] CompileFailurePoint test_failure_point(std::string_view sksl) noexcept {
    if (sksl.find("/*__NATIVEUI_T079_FAIL_DIAGNOSTIC__*/") != std::string_view::npos) {
        return CompileFailurePoint::BeforeDiagnosticOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T079_FAIL_AFTER_DIAGNOSTIC__*/") != std::string_view::npos) {
        return CompileFailurePoint::AfterDiagnosticOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T079_FAIL_PROGRAM_DATA__*/") != std::string_view::npos) {
        return CompileFailurePoint::ProgramDataAllocation;
    }
    if (sksl.find("/*__NATIVEUI_T079_FAIL_WRAPPER__*/") != std::string_view::npos) {
        return CompileFailurePoint::BeforeProgramWrapperAllocation;
    }
    if (sksl.find("/*__NATIVEUI_T079_FAIL_PUBLICATION__*/") != std::string_view::npos) {
        return CompileFailurePoint::ProgramPublicationAllocation;
    }
    if (sksl.find("/*__NATIVEUI_T079_EMPTY_DIAGNOSTIC__*/") != std::string_view::npos) {
        return CompileFailurePoint::EmptyBackendDiagnostic;
    }
    return CompileFailurePoint::None;
}

template <class T>
class FaultAllocator {
public:
    using value_type = T;

    explicit FaultAllocator(bool fail_allocation = false) noexcept
        : fail_allocation_(fail_allocation) {}

    template <class U>
    FaultAllocator(const FaultAllocator<U>& other) noexcept
        : fail_allocation_(other.fail_allocation_) {}

    [[nodiscard]] T* allocate(std::size_t count) {
        if (fail_allocation_) throw std::bad_alloc{};
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template <class U>
    [[nodiscard]] bool operator==(const FaultAllocator<U>& other) const noexcept {
        return fail_allocation_ == other.fail_allocation_;
    }

private:
    template <class>
    friend class FaultAllocator;

    bool fail_allocation_{};
};

[[nodiscard]] std::shared_ptr<const ShaderProgram> publish_fault_program(
    std::unique_ptr<ShaderProgram> candidate,
    bool fail_control_block_allocation) {
    auto* raw = candidate.release();

    // shared_ptr(Y*, D, A) invokes D(raw) if control-block allocation throws.
    return std::shared_ptr<const ShaderProgram>{
        raw,
        std::default_delete<ShaderProgram>{},
        FaultAllocator<ShaderProgram>{fail_control_block_allocation},
    };
}

#endif

} // namespace

} // namespace ui::detail

namespace ui {

ShaderProgram::ShaderProgram(
    std::shared_ptr<const detail::ShaderProgramData> data) noexcept
    : data_(std::move(data)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

ShaderCompileResult ShaderProgram::compile(std::string_view sksl) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    const auto failure = detail::test_failure_point(sksl);
#endif

    auto backend = SkRuntimeEffect::MakeForShader(SkString{sksl});

    if (!backend.effect) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
        if (failure == detail::CompileFailurePoint::BeforeDiagnosticOwnership) {
            throw std::bad_alloc{};
        }
        if (failure == detail::CompileFailurePoint::EmptyBackendDiagnostic) {
            backend.errorText.reset();
        }
#endif

        auto diagnostic = detail::compiler_diagnostic(backend.errorText);

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
        if (failure == detail::CompileFailurePoint::AfterDiagnosticOwnership) {
            throw std::bad_alloc{};
        }
#endif

        std::vector<ShaderDiagnostic> diagnostics;
        diagnostics.reserve(1);
        diagnostics.push_back(std::move(diagnostic));
        return ShaderCompileResult{nullptr, std::move(diagnostics)};
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    auto mutable_data = std::allocate_shared<detail::ShaderProgramData>(
        detail::FaultAllocator<detail::ShaderProgramData>{
            failure == detail::CompileFailurePoint::ProgramDataAllocation},
        std::move(backend.effect));
#else
    auto mutable_data =
        std::make_shared<detail::ShaderProgramData>(std::move(backend.effect));
#endif
    std::shared_ptr<const detail::ShaderProgramData> data = std::move(mutable_data);

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::CompileFailurePoint::BeforeProgramWrapperAllocation) {
        throw std::bad_alloc{};
    }
#endif

    auto candidate =
        std::unique_ptr<ShaderProgram>{new ShaderProgram{std::move(data)}};

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    auto program = detail::publish_fault_program(
        std::move(candidate),
        failure == detail::CompileFailurePoint::ProgramPublicationAllocation);
#else
    std::shared_ptr<const ShaderProgram> program{std::move(candidate)};
#endif
    return ShaderCompileResult{std::move(program), {}};
}

} // namespace ui
