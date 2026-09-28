#pragma once

/// \file
/// Public runtime-shader compilation, reflection and typed binding API.
///
/// Shader source compilation and ShaderInstance preparation may allocate and
/// belong to UI/resource-preparation code, never an audio real-time callback.
/// ShaderProgram is immutable after successful compilation. ShaderInstance is a
/// mutable, unsynchronized value owned by its caller; snapshot it into a Brush
/// before retained painting when a stable render value is required.

#include <nativeui/geometry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ui {

class Brush;

/// Stable category for an expected shader compilation/profile failure.
enum class ShaderCompileError {
    /// No error. Successful compilation normally returns no diagnostics.
    None,
    /// SkSL source could not be compiled, including the backend source-size limit.
    CompileError,
    /// Source compiled, but its reflected public interface is unsupported.
    UnsupportedInterface,
};

/// Owned diagnostic returned by ShaderProgram::compile().
struct ShaderDiagnostic {
    /// Machine-readable failure category.
    ShaderCompileError code{};
    /// 1-based source line when supplied by the backend; 0 means unavailable.
    int line{};
    /// 1-based source column when supplied by the backend; 0 means unavailable.
    int column{};
    /// Owned human-readable diagnostic; safe after the source/compiler is gone.
    std::string message;
};

class ShaderProgram;

/// Result of explicit shader compilation and interface validation.
struct ShaderCompileResult {
    /// Immutable compiled program on success; null on expected compile/profile failure.
    std::shared_ptr<const ShaderProgram> program;
    /// Owned diagnostics for expected failures; empty on normal success.
    std::vector<ShaderDiagnostic> diagnostics;

    /// Returns true exactly when a compiled program was published.
    [[nodiscard]] bool ok() const noexcept {
        return program != nullptr;
    }
};

/// NativeUI-supported reflected uniform types.
enum class ShaderUniformType {
    /// One finite floating-point scalar.
    Float,
    /// Two finite floating-point components.
    Float2,
    /// Three finite floating-point components.
    Float3,
    /// Four finite floating-point components without color semantics.
    Float4,
    /// One signed 32-bit integer.
    Int,
    /// Two signed 32-bit integers.
    Int2,
    /// Three signed 32-bit integers.
    Int3,
    /// Four signed 32-bit integers.
    Int4,
    /// A `layout(color)` float4/half4 value; set with set_color().
    Color,
};

/// Owned reflection record for one numeric/color uniform.
struct ShaderUniformInfo {
    /// Exact SkSL uniform name copied during compilation.
    std::string name;
    /// Setter family required for this slot.
    ShaderUniformType type{};
};

/// Owned reflection record for one `uniform shader` child slot.
struct ShaderChildInfo {
    /// Exact SkSL child name copied during compilation.
    std::string name;
};

/// Result of a typed ShaderInstance binding mutation.
enum class ShaderSetResult {
    /// The named slot existed and was updated.
    Ok,
    /// No matching slot exists, or the instance is moved-from/inert.
    NotFound,
    /// A numeric/color slot exists under the name but has a different reflected type.
    TypeMismatch,
    /// The value is non-finite, violates nesting limits, or cannot satisfy the slot contract.
    InvalidValue,
};

namespace detail {
struct ShaderProgramAccess;
struct ShaderProgramData;
struct ShaderInstanceAccess;
struct ShaderInstanceChildState;
} // namespace detail

/// Immutable compiled runtime-shader program.
///
/// Compilation is explicit resource-preparation work. It may allocate and is
/// not an audio-real-time operation. The returned program owns its backend
/// representation and NativeUI-owned reflection metadata; it never borrows the
/// caller's SkSL source or backend reflection string_views. Rendering never
/// invokes compilation implicitly. Backend/compiler types remain private to
/// NativeUI.
class ShaderProgram final {
public:
    ShaderProgram() = delete;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;
    ~ShaderProgram() noexcept;

    /// Compiles one SkSL runtime-shader program and freezes its supported interface.
    ///
    /// `sksl` is borrowed only for this call; the successful program owns all
    /// backend state and copied reflection names. Expected SkSL compile failures
    /// return a null program plus `CompileError`. A source that compiles but
    /// contains an unsupported reflected interface returns
    /// `UnsupportedInterface`.
    ///
    /// Supported uniforms are scalar/vector float and int values,
    /// `layout(color)` float4/half4, plus `uniform shader` children. Arrays,
    /// matrices and non-shader child interfaces are rejected. Compilation may
    /// allocate and may propagate allocation failures; invariant mismatches in
    /// the pinned backend interface are exceptional implementation failures.
    /// Rendering never recompiles this source implicitly.
    [[nodiscard]] static ShaderCompileResult compile(std::string_view sksl);

    /// Returns a borrowed read-only view of numeric/color reflection records.
    ///
    /// The span and its strings remain valid while this ShaderProgram remains
    /// alive and must not be retained past program destruction.
    [[nodiscard]] std::span<const ShaderUniformInfo> uniforms() const noexcept;

    /// Returns a borrowed read-only view of reflected `uniform shader` slots.
    ///
    /// The span and names remain valid while this ShaderProgram remains alive.
    [[nodiscard]] std::span<const ShaderChildInfo> children() const noexcept;

private:
    explicit ShaderProgram(std::unique_ptr<const detail::ShaderProgramData> data) noexcept;

    friend class ShaderInstance;
    friend struct detail::ShaderProgramAccess;
    std::unique_ptr<const detail::ShaderProgramData> data_;
};

/// Mutable logical bindings for one immutable ShaderProgram.
///
/// Construction prepares and zero-initializes the complete per-instance uniform
/// byte block plus one logical slot per reflected shader child. Numeric uniform
/// setters remain allocation-free/noexcept and create no backend resource.
/// set_child() snapshots a Brush value and may allocate, but performs no source
/// compilation or backend/context resource creation. Mutation is ordinary
/// UI/resource-preparation work; concurrent mutation of one ShaderInstance is
/// not synchronized.
class ShaderInstance final {
public:
    ShaderInstance() = delete;

    /// Creates an independent mutable binding set for `program`.
    ///
    /// The shared_ptr is retained. A null program throws std::invalid_argument.
    /// Numeric/color storage is zero-initialized and child slots begin unbound;
    /// an unbound shader child materializes as transparent black. Construction
    /// may allocate and is not an audio-real-time operation.
    explicit ShaderInstance(std::shared_ptr<const ShaderProgram> program);

    /// Copies bindings and child snapshots while sharing the immutable program.
    /// Later mutations of either instance are independent. May allocate.
    ShaderInstance(const ShaderInstance& other);
    /// Replaces this binding set with an independent copy. May allocate.
    ShaderInstance& operator=(const ShaderInstance& other);

    /// Transfers program and bindings without allocation; `other` becomes inert.
    ShaderInstance(ShaderInstance&& other) noexcept;
    /// Transfers program and bindings; self-move is a no-op and `other` becomes inert.
    ShaderInstance& operator=(ShaderInstance&& other) noexcept;
    /// Releases owned binding state and the shared immutable program reference.
    ~ShaderInstance() noexcept;

    /// True when this instance still owns a program; false for moved-from instances.
    [[nodiscard]] bool valid() const noexcept;

    /// Sets a finite scalar float uniform named `name`.
    ///
    /// Returns NotFound when absent/inert, TypeMismatch for any non-Float slot,
    /// and InvalidValue for NaN/Inf. On failure existing bytes are unchanged.
    ShaderSetResult set_float(std::string_view name, float value) noexcept;
    /// Sets a finite Float2 uniform. Failure is non-mutating.
    ShaderSetResult set_float2(std::string_view name, std::array<float, 2> value) noexcept;
    /// Sets a finite Float3 uniform. Failure is non-mutating.
    ShaderSetResult set_float3(std::string_view name, std::array<float, 3> value) noexcept;
    /// Sets a finite Float4 uniform. A Color slot deliberately requires set_color().
    ShaderSetResult set_float4(std::string_view name, std::array<float, 4> value) noexcept;
    /// Sets an Int uniform from an exact signed 32-bit value.
    ShaderSetResult set_int(std::string_view name, std::int32_t value) noexcept;
    /// Sets an Int2 uniform.
    ShaderSetResult set_int2(std::string_view name, std::array<std::int32_t, 2> value) noexcept;
    /// Sets an Int3 uniform.
    ShaderSetResult set_int3(std::string_view name, std::array<std::int32_t, 3> value) noexcept;
    /// Sets an Int4 uniform.
    ShaderSetResult set_int4(std::string_view name, std::array<std::int32_t, 4> value) noexcept;

    /// Sets a reflected Color slot from four finite channels.
    ///
    /// Values are not clamped by this setter; NaN/Inf in any channel returns
    /// InvalidValue and preserves the previous binding.
    ShaderSetResult set_color(std::string_view name, Color value) noexcept;

    /// Snapshots `brush` into the named `uniform shader` child slot.
    ///
    /// `name` and `brush` are borrowed only for this call. The instance
    /// retains an immutable Brush copy, so later source-Brush changes/lifetime
    /// do not affect the slot. NotFound means no child slot exists or the
    /// instance is inert. InvalidValue means the resulting nested shader depth
    /// would exceed kMaxChildDepth; the previous child is preserved.
    ///
    /// The successful path may allocate and may propagate std::bad_alloc.
    /// This operation is not synchronized with concurrent mutation.
    ShaderSetResult set_child(std::string_view name, const Brush& brush);

    /// Maximum materialized shader-brush nesting depth, counting the leaf as 1.
    /// Binding a child that would produce depth greater than 16 is rejected.
    static constexpr std::size_t kMaxChildDepth = 16;

    /// Returns a borrowed reference to the retained immutable program pointer.
    ///
    /// Copy the shared_ptr if ownership must outlive this ShaderInstance.
    /// A moved-from instance returns a reference to an empty shared_ptr.
    [[nodiscard]] const std::shared_ptr<const ShaderProgram>& program() const noexcept;

private:
    friend class Brush;
    friend struct detail::ShaderInstanceAccess;

    [[nodiscard]] static std::size_t prepared_binding_size(
        const std::shared_ptr<const ShaderProgram>& program);

    ShaderSetResult set_value(std::string_view name,
                              ShaderUniformType expected_type,
                              const void* bytes,
                              std::size_t byte_count,
                              bool value_valid) noexcept;

    [[nodiscard]] std::span<const std::byte> binding_bytes() const noexcept;
    void swap(ShaderInstance& other) noexcept;

    std::shared_ptr<const ShaderProgram> program_;
    std::vector<std::byte> bindings_;
    std::unique_ptr<detail::ShaderInstanceChildState> child_state_;
    std::size_t depth_{};
};

static_assert(!std::is_default_constructible_v<ShaderInstance>);
static_assert(std::is_nothrow_move_constructible_v<ShaderInstance>);
static_assert(std::is_nothrow_move_assignable_v<ShaderInstance>);
static_assert(std::is_nothrow_destructible_v<ShaderInstance>);

} // namespace ui
