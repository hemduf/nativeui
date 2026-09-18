#include <nativeui/shader.hpp>

#include <cstdint>
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
static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int(
    std::string_view{}, std::int32_t{})));

void nativeui_header_compile_shader() {}
