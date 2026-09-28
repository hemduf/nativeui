#include <nativeui/shader.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(!std::is_default_constructible_v<ui::ShaderProgram>);
static_assert(!std::is_copy_constructible_v<ui::ShaderProgram>);
static_assert(!std::is_move_constructible_v<ui::ShaderProgram>);
static_assert(std::is_nothrow_destructible_v<ui::ShaderProgram>);

static_assert(!std::is_default_constructible_v<ui::ShaderInstance>);
static_assert(std::is_copy_constructible_v<ui::ShaderInstance>);
static_assert(std::is_copy_assignable_v<ui::ShaderInstance>);
static_assert(std::is_nothrow_move_constructible_v<ui::ShaderInstance>);
static_assert(std::is_nothrow_move_assignable_v<ui::ShaderInstance>);
static_assert(std::is_nothrow_destructible_v<ui::ShaderInstance>);
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float(
    std::declval<std::string_view>(), std::declval<float>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float2(
    std::declval<std::string_view>(), std::declval<std::array<float, 2>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float3(
    std::declval<std::string_view>(), std::declval<std::array<float, 3>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float4(
    std::declval<std::string_view>(), std::declval<std::array<float, 4>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int(
    std::declval<std::string_view>(), std::declval<std::int32_t>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int2(
    std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 2>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int3(
    std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 3>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int4(
    std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 4>>())));
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_color(
    std::declval<std::string_view>(), std::declval<ui::Color>())));

void nativeui_header_compile_shader() {}

static_assert(ui::ShaderInstance::kMaxChildDepth == 16U);
static_assert(!noexcept(std::declval<ui::ShaderInstance&>().set_child(
    std::declval<std::string_view>(), std::declval<const ui::Brush&>())));
