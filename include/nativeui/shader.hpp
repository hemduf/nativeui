#pragma once

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

enum class ShaderCompileError {
    None,
    CompileError,
    UnsupportedInterface,
};

struct ShaderDiagnostic {
    ShaderCompileError code{};
    int line{};
    int column{};
    std::string message;
};

class ShaderProgram;

struct ShaderCompileResult {
    std::shared_ptr<const ShaderProgram> program;
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return program != nullptr;
    }
};

enum class ShaderUniformType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    Color,
};

struct ShaderUniformInfo {
    std::string name;
    ShaderUniformType type{};
};

struct ShaderChildInfo {
    std::string name;
};

enum class ShaderSetResult {
    Ok,
    NotFound,
    TypeMismatch,
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

    [[nodiscard]] static ShaderCompileResult compile(std::string_view sksl);
    [[nodiscard]] std::span<const ShaderUniformInfo> uniforms() const noexcept;
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
    explicit ShaderInstance(std::shared_ptr<const ShaderProgram> program);

    ShaderInstance(const ShaderInstance& other);
    ShaderInstance& operator=(const ShaderInstance& other);
    ShaderInstance(ShaderInstance&& other) noexcept;
    ShaderInstance& operator=(ShaderInstance&& other) noexcept;
    ~ShaderInstance() noexcept;

    [[nodiscard]] bool valid() const noexcept;

    ShaderSetResult set_float(std::string_view name, float value) noexcept;
    ShaderSetResult set_float2(std::string_view name, std::array<float, 2> value) noexcept;
    ShaderSetResult set_float3(std::string_view name, std::array<float, 3> value) noexcept;
    ShaderSetResult set_float4(std::string_view name, std::array<float, 4> value) noexcept;
    ShaderSetResult set_int(std::string_view name, std::int32_t value) noexcept;
    ShaderSetResult set_int2(std::string_view name, std::array<std::int32_t, 2> value) noexcept;
    ShaderSetResult set_int3(std::string_view name, std::array<std::int32_t, 3> value) noexcept;
    ShaderSetResult set_int4(std::string_view name, std::array<std::int32_t, 4> value) noexcept;
    ShaderSetResult set_color(std::string_view name, Color value) noexcept;
    ShaderSetResult set_child(std::string_view name, const Brush& brush);

    static constexpr std::size_t kMaxChildDepth = 16;

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
