#include <nativeui/paint_style.hpp>
#include <nativeui/shader.hpp>

#include "detail/shader_brush_access.hpp"
#include "detail/shader_instance_access.hpp"
#include "detail/shader_test_seams.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkShader.h"
#include "include/core/SkString.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkRuntimeEffect.h"

#include <algorithm>
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

[[nodiscard]] sk_sp<SkShader> materialize_image_texture(const ImageTexture& texture);

struct ShaderUniformSlot final {
    ShaderUniformType type{};
    std::size_t offset{};
    std::size_t size{};
};

struct ShaderProgramData final {
    // Keep the compiled backend object structurally immutable. NativeUI owns
    // copied uniform/child reflection metadata; backend/compiler types remain
    // private and are never exposed through the public shader API.
    ShaderProgramData(sk_sp<SkRuntimeEffect>&& effect_in,
                      std::vector<ShaderUniformInfo>&& uniforms_in,
                      std::vector<ShaderUniformSlot>&& slots_in,
                      std::vector<ShaderChildInfo>&& children_in,
                      std::size_t uniform_size_in)
        : effect(std::move(effect_in)),
          uniforms(std::move(uniforms_in)),
          slots(std::move(slots_in)),
          children(std::move(children_in)),
          uniform_size(uniform_size_in) {}

    sk_sp<const SkRuntimeEffect> effect;
    std::vector<ShaderUniformInfo> uniforms;
    std::vector<ShaderUniformSlot> slots;
    std::vector<ShaderChildInfo> children;
    std::size_t uniform_size{};
};

struct ShaderInstanceChildState final {
    explicit ShaderInstanceChildState(std::size_t child_count)
        : children(child_count) {}

    ShaderInstanceChildState(const ShaderInstanceChildState&) = default;
    ShaderInstanceChildState& operator=(const ShaderInstanceChildState&) = default;

    std::vector<std::shared_ptr<const Brush>> children;
};

struct ShaderBrushSnapshot final {
    ShaderBrushSnapshot(std::shared_ptr<const ShaderProgram> program_in,
                        std::span<const std::byte> bindings_in,
                        std::vector<std::shared_ptr<const Brush>>&& children_in,
                        std::size_t depth_in)
        : program(std::move(program_in)),
          bindings(bindings_in.size(), std::byte{0}),
          children(std::move(children_in)),
          depth(depth_in) {
        if (!bindings_in.empty()) {
            std::memcpy(bindings.data(), bindings_in.data(), bindings_in.size());
        }
    }

    std::shared_ptr<const ShaderProgram> program;
    std::vector<std::byte> bindings;
    std::vector<std::shared_ptr<const Brush>> children;
    std::size_t depth{1};
};

struct ShaderProgramAccess final {
    [[nodiscard]] static const ShaderProgramData& data(
        const ShaderProgram& program) noexcept {
        return *program.data_;
    }
};

static_assert(std::is_nothrow_destructible_v<SkRuntimeEffect>,
              "ShaderProgram noexcept teardown requires nothrow SkRuntimeEffect destruction");
static_assert(std::is_nothrow_destructible_v<sk_sp<const SkRuntimeEffect>>,
              "ShaderProgram noexcept teardown requires nothrow sk_sp destruction");
static_assert(std::is_nothrow_destructible_v<ShaderProgramData>,
              "ShaderProgramData must remain nothrow destructible");
static_assert(sizeof(std::int32_t) == sizeof(int),
              "NativeUI ShaderInstance requires pinned SkSL int to match int32_t");
static_assert(std::numeric_limits<int>::min() == std::numeric_limits<std::int32_t>::min() &&
              std::numeric_limits<int>::max() == std::numeric_limits<std::int32_t>::max(),
              "NativeUI ShaderInstance requires backend int to have the full int32_t range");

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
        case ShaderUniformType::Int: return sizeof(int);
        case ShaderUniformType::Int2: return sizeof(int) * 2U;
        case ShaderUniformType::Int3: return sizeof(int) * 3U;
        case ShaderUniformType::Int4: return sizeof(int) * 4U;
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

template <class T, std::size_t N>
[[nodiscard]] constexpr std::size_t packed_array_byte_size(
    const std::array<T, N>&) noexcept {
    return sizeof(T) * N;
}

[[nodiscard]] bool color_finite(Color color) noexcept {
    return std::isfinite(color.r) &&
           std::isfinite(color.g) &&
           std::isfinite(color.b) &&
           std::isfinite(color.a);
}


template <std::size_t N>
[[nodiscard]] std::array<int, N> backend_ints(
    const std::array<std::int32_t, N>& values) noexcept {
    std::array<int, N> converted{};
    for (std::size_t index = 0; index < N; ++index) {
        converted[index] = static_cast<int>(values[index]);
    }
    return converted;
}

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

std::size_t shader_compile_call_count_for_test_value = 0;

std::size_t shader_materialization_call_count_for_test_value = 0;
std::size_t shader_depth_read_call_count_for_test_value = 0;
ShaderMaterializationFailurePoint shader_materialization_failure_point_for_test_value =
    ShaderMaterializationFailurePoint::None;
std::size_t shader_materialization_failure_call_for_test_value = 0;

[[nodiscard]] bool consume_materialization_failure(
    ShaderMaterializationFailurePoint point) noexcept {
    if (shader_materialization_failure_point_for_test_value != point ||
        shader_materialization_call_count_for_test_value !=
            shader_materialization_failure_call_for_test_value) {
        return false;
    }
    shader_materialization_failure_point_for_test_value =
        ShaderMaterializationFailurePoint::None;
    shader_materialization_failure_call_for_test_value = 0;
    return true;
}

enum class CompileFailurePoint {
    None,
    BeforeDiagnosticOwnership,
    AfterDiagnosticOwnership,
    BeforeReflectionOwnership,
    DuringReflectionOwnership,
    AfterReflectionOwnership,
    BeforeUnsupportedDiagnostic,
    BeforeChildReflectionOwnership,
    DuringChildReflectionOwnership,
    AfterChildReflectionOwnership,
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
    if (sksl.find("/*__NATIVEUI_T080_FAIL_DURING_REFLECTION__*/") != std::string_view::npos) {
        return CompileFailurePoint::DuringReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T080_FAIL_AFTER_REFLECTION__*/") != std::string_view::npos) {
        return CompileFailurePoint::AfterReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T080_FAIL_UNSUPPORTED_DIAGNOSTIC__*/") !=
        std::string_view::npos) {
        return CompileFailurePoint::BeforeUnsupportedDiagnostic;
    }
    if (sksl.find("/*__NATIVEUI_T082_FAIL_CHILD_REFLECTION__*/") !=
        std::string_view::npos) {
        return CompileFailurePoint::BeforeChildReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T082_FAIL_DURING_CHILD_REFLECTION__*/") !=
        std::string_view::npos) {
        return CompileFailurePoint::DuringChildReflectionOwnership;
    }
    if (sksl.find("/*__NATIVEUI_T082_FAIL_AFTER_CHILD_REFLECTION__*/") !=
        std::string_view::npos) {
        return CompileFailurePoint::AfterChildReflectionOwnership;
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

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
std::size_t shader_compile_call_count_for_test() noexcept {
    return shader_compile_call_count_for_test_value;
}

std::size_t shader_materialization_call_count_for_test() noexcept {
    return shader_materialization_call_count_for_test_value;
}

std::size_t shader_depth_read_call_count_for_test() noexcept {
    return shader_depth_read_call_count_for_test_value;
}

void set_shader_materialization_failure_for_test(
    ShaderMaterializationFailurePoint point) noexcept {
    shader_materialization_failure_point_for_test_value = point;
    shader_materialization_failure_call_for_test_value =
        shader_materialization_call_count_for_test_value + 1U;
}

void set_shader_materialization_failure_after_calls_for_test(
    ShaderMaterializationFailurePoint point,
    std::size_t call_offset) noexcept {
    shader_materialization_failure_point_for_test_value = point;
    shader_materialization_failure_call_for_test_value =
        shader_materialization_call_count_for_test_value +
        (call_offset == 0U ? 1U : call_offset);
}
#endif

bool ShaderBrushAccess::is_shader(const Brush& brush) noexcept {
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    return snapshot != nullptr && static_cast<bool>(*snapshot);
}

bool ShaderBrushAccess::is_transparent_solid(const Brush& brush) noexcept {
    const auto* color = std::get_if<Color>(&brush.value_);
    return color != nullptr &&
           color->r == 0.0f &&
           color->g == 0.0f &&
           color->b == 0.0f &&
           color->a == 0.0f;
}

const ShaderProgram* ShaderBrushAccess::program(const Brush& brush) noexcept {
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    return snapshot != nullptr && *snapshot ? (*snapshot)->program.get() : nullptr;
}

std::span<const std::byte> ShaderBrushAccess::binding_bytes(
    const Brush& brush) noexcept {
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    if (snapshot == nullptr || !*snapshot) return {};
    return {(*snapshot)->bindings.data(), (*snapshot)->bindings.size()};
}

std::size_t ShaderBrushAccess::depth(const Brush& brush) noexcept {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    ++shader_depth_read_call_count_for_test_value;
#endif
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    return snapshot != nullptr && *snapshot ? (*snapshot)->depth : 0U;
}

std::size_t ShaderBrushAccess::child_count(const Brush& brush) noexcept {
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    return snapshot != nullptr && *snapshot ? (*snapshot)->children.size() : 0U;
}

const Brush* ShaderBrushAccess::child(const Brush& brush, std::size_t index) noexcept {
    const auto* snapshot =
        std::get_if<std::shared_ptr<const ShaderBrushSnapshot>>(&brush.value_);
    if (snapshot == nullptr || !*snapshot || index >= (*snapshot)->children.size()) {
        return nullptr;
    }
    const auto& slot = (*snapshot)->children[index];
    return slot.get();
}

std::size_t ShaderInstanceAccess::child_count(
    const ShaderInstance& instance) noexcept {
    return instance.child_state_ ? instance.child_state_->children.size() : 0U;
}

const Brush* ShaderInstanceAccess::child(
    const ShaderInstance& instance,
    std::size_t index) noexcept {
    if (!instance.child_state_ ||
        index >= instance.child_state_->children.size()) {
        return nullptr;
    }
    const auto& slot = instance.child_state_->children[index];
    return slot.get();
}

std::size_t ShaderInstanceAccess::depth(
    const ShaderInstance& instance) noexcept {
    return instance.depth_;
}

namespace {

[[nodiscard]] SkColor4f child_sk_color(Color color) noexcept {
    return SkColor4f{color.r, color.g, color.b, color.a};
}

[[nodiscard]] sk_sp<SkShader> child_color_shader(Color color) {
    return SkShaders::Color(child_sk_color(color), nullptr);
}

[[nodiscard]] sk_sp<SkShader> child_gradient_fallback_shader(Color color) {
    // T073 direct gradient fallback stores the fallback RGB in SkPaint and then
    // PaintOptions replaces the paint alpha. A child shader has no inner paint,
    // so force fallback alpha to 1 and let the outer parent PaintOptions apply
    // the one and only draw alpha.
    color.a = 1.0f;
    return child_color_shader(color);
}

[[nodiscard]] bool child_valid_gradient_stops(
    const std::vector<GradientStop>& stops) noexcept {
    if (stops.size() < 2U) return false;
    float previous = -1.0f;
    for (const auto& stop : stops) {
        if (!std::isfinite(stop.offset) ||
            stop.offset < 0.0f ||
            stop.offset > 1.0f ||
            stop.offset <= previous) {
            return false;
        }
        previous = stop.offset;
    }
    return true;
}

template <class Factory>
[[nodiscard]] sk_sp<SkShader> child_gradient_shader(
    const std::vector<GradientStop>& stops,
    Factory&& factory) {
    if (stops.empty()) return child_color_shader(Color{});
    if (!child_valid_gradient_stops(stops)) {
        return child_gradient_fallback_shader(stops.front().color);
    }

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    colors.reserve(stops.size());
    positions.reserve(stops.size());
    for (const auto& stop : stops) {
        colors.push_back(child_sk_color(stop.color));
        positions.push_back(stop.offset);
    }

    const SkGradient gradient{
        {{colors.data(), colors.size()},
         {positions.data(), positions.size()},
         SkTileMode::kClamp},
        {}};
    auto shader = std::forward<Factory>(factory)(gradient);
    return shader ? std::move(shader)
                  : child_gradient_fallback_shader(stops.front().color);
}

[[nodiscard]] sk_sp<SkShader> materialize_shader_snapshot(
    const std::shared_ptr<const ShaderBrushSnapshot>& snapshot);

} // namespace

struct ShaderBrushMaterializer final {
    [[nodiscard]] static std::size_t depth(const Brush& brush) noexcept {
        return ShaderBrushAccess::depth(brush);
    }

    [[nodiscard]] static bool requires_linear_color_working_space(
        const Brush& brush) noexcept {
        return std::visit(
            [](const auto& source) noexcept -> bool {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, ImageTexture>) {
                    return source.interpretation() == TextureInterpretation::Color;
                } else if constexpr (
                    std::is_same_v<
                        Source,
                        std::shared_ptr<const ShaderBrushSnapshot>>) {
                    if (!source) return false;
                    for (const auto& child : source->children) {
                        if (child &&
                            ShaderBrushMaterializer::
                                requires_linear_color_working_space(*child)) {
                            return true;
                        }
                    }
                    return false;
                } else {
                    return false;
                }
            },
            brush.value_);
    }

    [[nodiscard]] static sk_sp<SkShader> materialize(const Brush& brush) {
        return std::visit(
            [](const auto& source) -> sk_sp<SkShader> {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, Color>) {
                    return child_color_shader(source);
                } else if constexpr (std::is_same_v<Source, LinearGradient>) {
                    const auto start = source.start();
                    const auto end = source.end();
                    const SkPoint points[2]{{start.x, start.y}, {end.x, end.y}};
                    return child_gradient_shader(
                        source.stops(),
                        [points](const SkGradient& gradient) {
                            return SkShaders::LinearGradient(points, gradient);
                        });
                } else if constexpr (std::is_same_v<Source, RadialGradient>) {
                    const auto center = source.center();
                    const SkPoint sk_center{center.x, center.y};
                    return child_gradient_shader(
                        source.stops(),
                        [sk_center, &source](const SkGradient& gradient) {
                            if (!(source.radius() > 0.0f) ||
                                !std::isfinite(source.radius())) {
                                return sk_sp<SkShader>{};
                            }
                            return SkShaders::RadialGradient(
                                sk_center, source.radius(), gradient);
                        });
                } else if constexpr (std::is_same_v<Source, ImageTexture>) {
                    return materialize_image_texture(source);
                } else {
                    return materialize_shader_snapshot(source);
                }
            },
            brush.value_);
    }
};

namespace {

[[nodiscard]] sk_sp<SkShader> materialize_shader_snapshot(
    const std::shared_ptr<const ShaderBrushSnapshot>& snapshot) {
    if (!snapshot || !snapshot->program) {
        throw std::runtime_error("NativeUI shader Brush snapshot is invalid");
    }
    if (snapshot->depth == 0U ||
        snapshot->depth > ShaderInstance::kMaxChildDepth) {
        throw std::runtime_error(
            "NativeUI shader Brush snapshot depth is invalid");
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    ++shader_materialization_call_count_for_test_value;
    if (consume_materialization_failure(
            ShaderMaterializationFailurePoint::BeforeUniformData)) {
        throw std::bad_alloc{};
    }
#endif

    const auto& data = ShaderProgramAccess::data(*snapshot->program);
    if (snapshot->bindings.size() != data.uniform_size) {
        throw std::runtime_error(
            "NativeUI shader Brush binding block size does not match program");
    }
    if (snapshot->children.size() != data.children.size()) {
        throw std::runtime_error(
            "NativeUI shader Brush child slot count does not match program");
    }

    sk_sp<SkData> uniforms;
    if (snapshot->bindings.empty()) {
        uniforms = SkData::MakeEmpty();
    } else {
        uniforms = SkData::MakeWithCopy(
            snapshot->bindings.data(),
            snapshot->bindings.size());
    }
    if (!uniforms) throw std::bad_alloc{};

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (consume_materialization_failure(
            ShaderMaterializationFailurePoint::BeforeChildren)) {
        throw std::bad_alloc{};
    }
#endif

    std::vector<SkRuntimeEffect::ChildPtr> backend_children;
    backend_children.reserve(snapshot->children.size());
    bool requires_linear_color_working_space = false;
    for (const auto& slot : snapshot->children) {
        if (!slot) {
            backend_children.emplace_back();
            continue;
        }
        requires_linear_color_working_space =
            requires_linear_color_working_space ||
            ShaderBrushMaterializer::requires_linear_color_working_space(*slot);
        auto child_shader = ShaderBrushMaterializer::materialize(*slot);
        if (!child_shader) {
            throw std::runtime_error(
                "NativeUI shader child materialization returned no shader");
        }
        backend_children.emplace_back(std::move(child_shader));
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (consume_materialization_failure(
            ShaderMaterializationFailurePoint::BeforeShader)) {
        throw std::bad_alloc{};
    }
#endif

    auto shader = data.effect->makeShader(
        std::move(uniforms),
        SkSpan<const SkRuntimeEffect::ChildPtr>{
            backend_children.data(), backend_children.size()});

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (consume_materialization_failure(
            ShaderMaterializationFailurePoint::ForceNullShader)) {
        shader.reset();
    }
#endif

    if (!shader) {
        throw std::runtime_error(
            "NativeUI runtime shader materialization returned no shader");
    }

    // A runtime effect that consumes a Color ImageTexture performs its
    // arithmetic in NativeUI's fixed linear-sRGB material working space.
    // Skia converts ordinary color children into that space and converts the
    // effect result back to the destination color space. Raw image shaders
    // deliberately ignore the working color space, so Data children retain
    // their codec-decoded numeric channels even in a mixed Color/Data effect.
    if (requires_linear_color_working_space) {
        shader = shader->makeWithWorkingColorSpace(
            SkColorSpace::MakeSRGBLinear());
        if (!shader) {
            throw std::runtime_error(
                "NativeUI linear-sRGB shader materialization returned no shader");
        }
    }
    return shader;
}

} // namespace

sk_sp<SkShader> materialize_shader_brush(
    const std::shared_ptr<const ShaderBrushSnapshot>& snapshot) {
    return materialize_shader_snapshot(snapshot);
}

} // namespace ui::detail

namespace ui {

Brush::Brush(const ShaderInstance& shader)
    : value_(transparent()) {
    if (!shader.valid()) return;

    const auto bindings = detail::ShaderInstanceAccess::binding_bytes(shader);
    std::vector<std::shared_ptr<const Brush>> children;
    if (shader.child_state_) {
        children = shader.child_state_->children;
    }

    std::shared_ptr<const detail::ShaderBrushSnapshot> snapshot =
        std::make_shared<detail::ShaderBrushSnapshot>(
            shader.program(),
            bindings,
            std::move(children),
            shader.depth_);
    value_ = Storage{std::move(snapshot)};
}

ShaderProgram::ShaderProgram(
    std::unique_ptr<const detail::ShaderProgramData> data) noexcept
    : data_(std::move(data)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

std::span<const ShaderUniformInfo> ShaderProgram::uniforms() const noexcept {
    return {data_->uniforms.data(), data_->uniforms.size()};
}

std::span<const ShaderChildInfo> ShaderProgram::children() const noexcept {
    return {data_->children.data(), data_->children.size()};
}

ShaderCompileResult ShaderProgram::compile(std::string_view sksl) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    ++detail::shader_compile_call_count_for_test_value;
#endif
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

    const auto reflected_children = backend.effect->children();
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::CompileFailurePoint::BeforeChildReflectionOwnership) {
        throw std::bad_alloc{};
    }
#endif

    std::vector<ShaderChildInfo> children;
    children.reserve(reflected_children.size());
    for (std::size_t index = 0; index < reflected_children.size(); ++index) {
        const auto& reflected = reflected_children[index];
        if (reflected.type != SkRuntimeEffect::ChildType::kShader) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
            if (failure == detail::CompileFailurePoint::BeforeUnsupportedDiagnostic) {
                throw std::bad_alloc{};
            }
#endif
            return detail::unsupported_interface_result(
                "Only shader child interfaces are supported by NativeUI T082");
        }
        if (reflected.index < 0 ||
            static_cast<std::size_t>(reflected.index) != index) {
            throw std::runtime_error(
                "Pinned Skia child reflection index/order does not match NativeUI T082");
        }

        children.push_back(ShaderChildInfo{std::string{reflected.name}});

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
        if (failure == detail::CompileFailurePoint::DuringChildReflectionOwnership &&
            children.size() == 1U) {
            throw std::bad_alloc{};
        }
#endif
    }

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    if (failure == detail::CompileFailurePoint::AfterChildReflectionOwnership) {
        throw std::bad_alloc{};
    }
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
        if (reflected.offset != previous_end ||
            reflected.offset > uniform_size ||
            reflected_size > uniform_size - reflected.offset) {
            throw std::runtime_error(
                "Pinned Skia uniform reflection is not exactly contiguous");
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

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
        if (failure == detail::CompileFailurePoint::DuringReflectionOwnership &&
            uniforms.size() == 1U) {
            throw std::bad_alloc{};
        }
#endif
    }

    if (previous_end != uniform_size) {
        throw std::runtime_error(
            "Pinned Skia uniform reflection does not cover the complete uniform block");
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
            std::move(children),
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
      bindings_(prepared_binding_size(program_), std::byte{0}),
      depth_(1U) {
    if (!program_->data_->children.empty()) {
        child_state_ = std::make_unique<detail::ShaderInstanceChildState>(
            program_->data_->children.size());
    }
}

ShaderInstance::ShaderInstance(const ShaderInstance& other)
    : program_(other.program_),
      depth_(other.program_ ? other.depth_ : 0U) {
    if (program_) {
        bindings_ = other.bindings_;
        if (other.child_state_) {
            child_state_ =
                std::make_unique<detail::ShaderInstanceChildState>(
                    *other.child_state_);
        }
    }
}

ShaderInstance& ShaderInstance::operator=(const ShaderInstance& other) {
    if (this == &other) return *this;
    if (!other.program_) {
        program_.reset();
        bindings_.clear();
        child_state_.reset();
        depth_ = 0U;
        return *this;
    }
    ShaderInstance replacement{other};
    swap(replacement);
    return *this;
}

ShaderInstance::ShaderInstance(ShaderInstance&& other) noexcept
    : program_(std::move(other.program_)),
      bindings_(std::move(other.bindings_)),
      child_state_(std::move(other.child_state_)),
      depth_(other.depth_) {
    other.bindings_.clear();
    other.depth_ = 0U;
}

ShaderInstance& ShaderInstance::operator=(ShaderInstance&& other) noexcept {
    if (this == &other) return *this;
    program_ = std::move(other.program_);
    bindings_ = std::move(other.bindings_);
    child_state_ = std::move(other.child_state_);
    depth_ = other.depth_;
    other.bindings_.clear();
    other.depth_ = 0U;
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
    child_state_.swap(other.child_state_);
    std::swap(depth_, other.depth_);
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

ShaderSetResult ShaderInstance::set_child(
    std::string_view name,
    const Brush& brush) {
    if (!program_) return ShaderSetResult::NotFound;

    const auto& data = *program_->data_;
    std::size_t child_index = data.children.size();
    for (std::size_t index = 0; index < data.children.size(); ++index) {
        if (std::string_view{data.children[index].name} == name) {
            child_index = index;
            break;
        }
    }
    if (child_index == data.children.size()) {
        return ShaderSetResult::NotFound;
    }
    if (!child_state_ ||
        child_state_->children.size() != data.children.size()) {
        throw std::runtime_error(
            "NativeUI ShaderInstance child slot state is inconsistent");
    }

    std::size_t max_child_depth = 0U;
    for (std::size_t index = 0; index < child_state_->children.size(); ++index) {
        const Brush* candidate = nullptr;
        if (index == child_index) {
            candidate = &brush;
        } else if (child_state_->children[index]) {
            candidate = &*child_state_->children[index];
        }
        if (candidate) {
            max_child_depth = std::max(
                max_child_depth,
                detail::ShaderBrushMaterializer::depth(*candidate));
        }
    }

    if (max_child_depth >= kMaxChildDepth) {
        return ShaderSetResult::InvalidValue;
    }

    auto replacement = std::make_shared<const Brush>(brush);
    child_state_->children[child_index] = std::move(replacement);
    depth_ = 1U + max_child_depth;
    return ShaderSetResult::Ok;
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
        detail::packed_array_byte_size(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_float3(
    std::string_view name,
    std::array<float, 3> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float3,
        value.data(),
        detail::packed_array_byte_size(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_float4(
    std::string_view name,
    std::array<float, 4> value) noexcept {
    return set_value(
        name,
        ShaderUniformType::Float4,
        value.data(),
        detail::packed_array_byte_size(value),
        detail::all_finite(value));
}

ShaderSetResult ShaderInstance::set_int(
    std::string_view name,
    std::int32_t value) noexcept {
    const int backend_value = static_cast<int>(value);
    return set_value(
        name,
        ShaderUniformType::Int,
        &backend_value,
        sizeof(backend_value),
        true);
}

ShaderSetResult ShaderInstance::set_int2(
    std::string_view name,
    std::array<std::int32_t, 2> value) noexcept {
    const auto backend_values = detail::backend_ints(value);
    return set_value(
        name,
        ShaderUniformType::Int2,
        backend_values.data(),
        detail::packed_array_byte_size(backend_values),
        true);
}

ShaderSetResult ShaderInstance::set_int3(
    std::string_view name,
    std::array<std::int32_t, 3> value) noexcept {
    const auto backend_values = detail::backend_ints(value);
    return set_value(
        name,
        ShaderUniformType::Int3,
        backend_values.data(),
        detail::packed_array_byte_size(backend_values),
        true);
}

ShaderSetResult ShaderInstance::set_int4(
    std::string_view name,
    std::array<std::int32_t, 4> value) noexcept {
    const auto backend_values = detail::backend_ints(value);
    return set_value(
        name,
        ShaderUniformType::Int4,
        backend_values.data(),
        detail::packed_array_byte_size(backend_values),
        true);
}

ShaderSetResult ShaderInstance::set_color(std::string_view name, Color value) noexcept {
    const std::array<float, 4> components{value.r, value.g, value.b, value.a};
    return set_value(
        name,
        ShaderUniformType::Color,
        components.data(),
        detail::packed_array_byte_size(components),
        detail::color_finite(value));
}

} // namespace ui
