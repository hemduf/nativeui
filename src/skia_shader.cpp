#include <nativeui/shader.hpp>

#include "include/core/SkString.h"
#include "include/effects/SkRuntimeEffect.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui::detail {

struct ShaderUniformSlot final {
    ShaderUniformType type{};
    std::size_t offset{};
    std::size_t size{};
};

struct ShaderProgramData final {
    // Keep the compiled backend object structurally immutable. T079/T080 only
    // consume const SkRuntimeEffect APIs, and the pinned m149 converting move
    // into sk_sp<const T> is not declared noexcept, so this constructor remains
    // potentially throwing rather than terminating the host.
    ShaderProgramData(sk_sp<SkRuntimeEffect>&& effect_in,
                      std::vector<ShaderUniformInfo>&& uniforms_in,
                      std::vector<ShaderUniformSlot>&& slots_in,
                      std::size_t uniform_size_in)
        : effect(std::move(effect_in)),
          uniforms(std::move(uniforms_in)),
          slots(std::move(slots_in)),
          uniform_size(uniform_size_in) {}

    sk_sp<const SkRuntimeEffect> effect;
    std::vector<ShaderUniformInfo> uniforms;
    std::vector<ShaderUniformSlot> slots;
    std::size_t uniform_size{};
};

static_assert(std::is_nothrow_destructible_v<SkRuntimeEffect>,
              "ShaderProgram noexcept teardown requires nothrow SkRuntimeEffect destruction");
static_assert(std::is_nothrow_destructible_v<sk_sp<const SkRuntimeEffect>>,
              "ShaderProgram noexcept teardown requires nothrow sk_sp destruction");
static_assert(std::is_nothrow_destructible_v<ShaderProgramData>,
              "ShaderProgramData must remain nothrow destructible");
static_assert(sizeof(std::int32_t) == sizeof(int),
              "NativeUI ShaderInstance requires pinned SkSL int to match int32_t");

namespace {

constexpr size_t kMaxSkSLSourceBytes =
    static_cast<size_t>(std::numeric_limits<uint32_t>::max());

[[nodiscard]] ShaderCompileResult oversized_source_result() {
    std::vector<ShaderDiagnostic> diagnostics;
    diagnostics.reserve(1);
    diagnostics.push_back(ShaderDiagnostic{
        ShaderCompileError::CompileError,
        0,
        0,
        "SkSL source exceeds the backend size limit",
    });
    return ShaderCompileResult{nullptr, std::move(diagnostics)};
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

[[nodiscard]] ShaderCompileResult unsupported_interface_result(const char* message) {
    std::vector<ShaderDiagnostic> diagnostics;
    diagnostics.reserve(1);
    diagnostics.push_back(ShaderDiagnostic{
        ShaderCompileError::UnsupportedInterface,
        0,
        0,
        message,
    });
    return ShaderCompileResult{nullptr, std::move(diagnostics)};
}

[[nodiscard]] std::size_t expected_uniform_size(ShaderUniformType type) noexcept {
    switch (type) {
        case ShaderUniformType::Float: return sizeof(float);
        case ShaderUniformType::Float2: return sizeof(float) * 2U;
        case ShaderUniformType::Float3: return sizeof(float) * 3U;
        case ShaderUniformType::Float4: return sizeof(float) * 4U;
        case ShaderUniformType::Int: return sizeof(std::int32_t);
        case ShaderUniformType::Int2: return sizeof(std::int32_t) * 2U;
        case ShaderUniformType::Int3: return sizeof(std::int32_t) * 3U;
        case ShaderUniformType::Int4: return sizeof(std::int32_t) * 4U;
        case ShaderUniformType::Color: return sizeof(float) * 4U;
    }
    return 0;
}

[[nodiscard]] bool reflected_uniform_type(
    const SkRuntimeEffect::Uniform& uniform,
    ShaderUniformType& type,
    const char*& unsupported_reason) noexcept {
    using BackendType = SkRuntimeEffect::Uniform::Type;
    using BackendFlags = SkRuntimeEffect::Uniform::Flags;

    if (uniform.isArray()) {
        unsupported_reason = "SkSL uniform arrays are unsupported by NativeUI T080";
        return false;
    }

    const bool is_color = (uniform.flags & BackendFlags::kColor_Flag) != 0U;
    if (is_color) {
        if (uniform.type == BackendType::kFloat4) {
            type = ShaderUniformType::Color;
            return true;
        }
        unsupported_reason =
            "Only non-array layout(color) float4/half4 uniforms are supported by NativeUI T080";
        return false;
    }

    switch (uniform.type) {
        case BackendType::kFloat: type = ShaderUniformType::Float; return true;
        case BackendType::kFloat2: type = ShaderUniformType::Float2; return true;
        case BackendType::kFloat3: type = ShaderUniformType::Float3; return true;
        case BackendType::kFloat4: type = ShaderUniformType::Float4; return true;
        case BackendType::kInt: type = ShaderUniformType::Int; return true;
        case BackendType::kInt2: type = ShaderUniformType::Int2; return true;
        case BackendType::kInt3: type = ShaderUniformType::Int3; return true;
        case BackendType::kInt4: type = ShaderUniformType::Int4; return true;
        case BackendType::kFloat2x2:
        case BackendType::kFloat3x3:
        case BackendType::kFloat4x4:
            unsupported_reason = "SkSL matrix uniforms are unsupported by NativeUI T080";
            return false;
    }

    unsupported_reason = "SkSL uniform type is unsupported by NativeUI T080";
    return false;
}

template <std::size_t N>
[[nodiscard]] bool all_finite(const std::array<float, N>& values) noexcept {
    for (float value : values) {
        if (!std::isfinite(value)) return false;
    }
    return true;
}

[[nodiscard]] bool color_finite(Color color) noexcept {
    return std::isfinite(color.r) &&
           std::isfinite(color.g) &&
           std::isfinite(color.b) &&
           std::isfinite(color.a);
}

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class CompileFailurePoint {
    None,
    BeforeDiagnosticOwnership,
    AfterDiagnosticOwnership,
    BeforeReflectionOwnership,
    AfterReflectionOwnership,
    BeforeUnsupportedDiagnostic,
    ProgramDataAllocation,
    BeforeProgramWrapperAllocation,
    ProgramPublicationAllocation,
    EmptyBackendDiagnostic,
    OversizedSource,
};

[[nodiscard]] CompileFailurePoint test_failure_point(std::string_view sksl) noexcept {
    if (sksl.find("/*__NATIVEUI_T079_FAIL_DIAGNOSTIC__*/") != std::string_view::npos) {
        return CompileFailurePoint::BeforeDiagnosticOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T079_FAIL_AFTER_DIAGNOSTIC__*/") != std::string_view::npos) {
        return CompileFailurePoint::AfterDiagnosticOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T080_FAIL_REFLECTION__*/") != std::string_view::npos) {
        return CompileFailurePoint::BeforeReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T080_FAIL_AFTER_REFLECTION__*/") != std::string_view::npos) {
        return CompileFailurePoint::AfterReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T080_FAIL_UNSUPPORTED_DIAGNOSTIC__*/") !=
        std::string_view::npos) {
        return CompileFailurePoint::BeforeUnsupportedDiagnostic;
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
    if (sksl.find("/*__NATIVEUI_T079_OVERSIZED_SOURCE__*/") != std::string_view::npos) {
        return CompileFailurePoint::OversizedSource;
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
    std::unique_ptr<const detail::ShaderProgramData> data) noexcept
    : data_(std::move(data)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

std::span<const ShaderUniformInfo> ShaderProgram::uniforms() const noexcept {
    return {data_->uniforms.data(), data_->uniforms.size()};
}

ShaderCompileResult ShaderProgram::compile(std::string_view sksl) {
    if (sksl.size() > detail::kMaxSkSLSourceBytes) {
        return detail::oversized_source_result();
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    const auto failure = detail::test_failure_point(sksl);
    if (failure == detail::CompileFailurePoint::OversizedSource) {
        return detail::oversized_source_result();
    }
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

    if (!backend.effect->children().empty()) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
        if (failure == detail::CompileFailurePoint::BeforeUnsupportedDiagnostic) {
            throw std::bad_alloc{};
        }
#endif
        return detail::unsupported_interface_result(
            "SkSL child interfaces are unsupported by NativeUI T080");
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::CompileFailurePoint::BeforeReflectionOwnership) {
        throw std::bad_alloc{};
    }
#endif

    const auto reflected_uniforms = backend.effect->uniforms();
    const std::size_t uniform_size = backend.effect->uniformSize();

    std::vector<ShaderUniformInfo> uniforms;
    std::vector<detail::ShaderUniformSlot> slots;
    uniforms.reserve(reflected_uniforms.size());
    slots.reserve(reflected_uniforms.size());

    std::size_t previous_end = 0;
    for (const auto& reflected : reflected_uniforms) {
        ShaderUniformType type{};
        const char* unsupported_reason = nullptr;
        if (!detail::reflected_uniform_type(reflected, type, unsupported_reason)) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
            if (failure == detail::CompileFailurePoint::BeforeUnsupportedDiagnostic) {
                throw std::bad_alloc{};
            }
#endif
            return detail::unsupported_interface_result(unsupported_reason);
        }

        const std::size_t expected_size = detail::expected_uniform_size(type);
        const std::size_t reflected_size = reflected.sizeInBytes();
        if (reflected_size != expected_size) {
            throw std::runtime_error(
                "Pinned Skia uniform byte size does not match NativeUI T080 packing");
        }
        if (reflected.offset < previous_end ||
            reflected.offset > uniform_size ||
            reflected_size > uniform_size - reflected.offset) {
            throw std::runtime_error(
                "Pinned Skia uniform reflection produced invalid or overlapping bounds");
        }

        uniforms.push_back(ShaderUniformInfo{
            std::string{reflected.name},
            type,
        });
        slots.push_back(detail::ShaderUniformSlot{
            type,
            reflected.offset,
            reflected_size,
        });
        previous_end = reflected.offset + reflected_size;
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::CompileFailurePoint::AfterReflectionOwnership) {
        throw std::bad_alloc{};
    }
    if (failure == detail::CompileFailurePoint::ProgramDataAllocation) {
        throw std::bad_alloc{};
    }
#endif

    std::unique_ptr<const detail::ShaderProgramData> data =
        std::make_unique<detail::ShaderProgramData>(
            std::move(backend.effect),
            std::move(uniforms),
            std::move(slots),
            uniform_size);

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

std::size_t ShaderInstance::prepared_binding_size(
    const std::shared_ptr<const ShaderProgram>& program) {
    if (!program) {
        throw std::invalid_argument("ShaderInstance requires a non-null ShaderProgram");
    }
    return program->data_->uniform_size;
}

ShaderInstance::ShaderInstance(std::shared_ptr<const ShaderProgram> program)
    : program_(std::move(program)),
      bindings_(prepared_binding_size(program_), std::byte{0}) {}

ShaderInstance::ShaderInstance(const ShaderInstance& other)
    : program_(other.program_),
      bindings_(other.bindings_) {}

ShaderInstance& ShaderInstance::operator=(const ShaderInstance& other) {
    if (this == &other) return *this;
    ShaderInstance replacement{other};
    swap(replacement);
    return *this;
}

ShaderInstance::ShaderInstance(ShaderInstance&& other) noexcept
    : program_(std::move(other.program_)),
      bindings_(std::move(other.bindings_)) {
    other.bindings_.clear();
}

ShaderInstance& ShaderInstance::operator=(ShaderInstance&& other) noexcept {
    if (this == &other) return *this;
    program_ = std::move(other.program_);
    bindings_ = std::move(other.bindings_);
    other.bindings_.clear();
    return *this;
}

ShaderInstance::~ShaderInstance() noexcept = default;

bool ShaderInstance::valid() const noexcept {
    return program_ != nullptr;
}

const std::shared_ptr<const ShaderProgram>& ShaderInstance::program() const noexcept {
    return program_;
}

std::span<const std::byte> ShaderInstance::binding_bytes() const noexcept {
    return {bindings_.data(), bindings_.size()};
}

void ShaderInstance::swap(ShaderInstance& other) noexcept {
    program_.swap(other.program_);
    bindings_.swap(other.bindings_);
}

ShaderSetResult ShaderInstance::set_value(std::string_view name,
                                         ShaderUniformType expected_type,
                                         const void* bytes,
                                         std::size_t byte_count,
                                         bool value_valid) noexcept {
    if (!program_) return ShaderSetResult::NotFound;

    const auto& data = *program_->data_;
    for (std::size_t index = 0; index < data.uniforms.size(); ++index) {
        if (std::string_view{data.uniforms[index].name} != name) continue;

        const auto& slot = data.slots[index];
        if (slot.type != expected_type) return ShaderSetResult::TypeMismatch;
        if (!value_valid) return ShaderSetResult::InvalidValue;

        if (slot.size != byte_count ||
            slot.offset > bindings_.size() ||
            byte_count > bindings_.size() - slot.offset) {
            return ShaderSetResult::InvalidValue;
        }

        std::memcpy(bindings_.data() + slot.offset, bytes, byte_count);
        return ShaderSetResult::Ok;
    }

    return ShaderSetResult::NotFound;
}

ShaderSetResult ShaderInstance::set_float(std::string_view name, float value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float,
        &value,
        sizeof(value),
        std::isfinite(value));
}

ShaderSetResult ShaderInstance::set_float2(
    std::string_view name,
    std::array<float, 2> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float2,
        value.data(),
        sizeof(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_float3(
    std::string_view name,
    std::array<float, 3> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float3,
        value.data(),
        sizeof(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_float4(
    std::string_view name,
    std::array<float, 4> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float4,
        value.data(),
        sizeof(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_int(
    std::string_view name,
    std::int32_t value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Int,
        &value,
        sizeof(value),
        true);
}

ShaderSetResult ShaderInstance::set_int2(
    std::string_view name,
    std::array<std::int32_t, 2> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Int2,
        value.data(),
        sizeof(value),
        true);
}

ShaderSetResult ShaderInstance::set_int3(
    std::string_view name,
    std::array<std::int32_t, 3> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Int3,
        value.data(),
        sizeof(value),
        true);
}

ShaderSetResult ShaderInstance::set_int4(
    std::string_view name,
    std::array<std::int32_t, 4> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Int4,
        value.data(),
        sizeof(value),
        true);
}

ShaderSetResult ShaderInstance::set_color(std::string_view name, Color value) noexcept {
    const std::array<float, 4> components{value.r, value.g, value.b, value.a};
    return set_value(
        name,
        ShaderUniformType::Color,
        components.data(),
        sizeof(components),
        detail::color_finite(value));
}

} // namespace ui
