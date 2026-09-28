#include <nativeui/shader.hpp>

#include <type_traits>

static_assert(!std::is_default_constructible_v<ui::ShaderProgram>);
static_assert(!std::is_copy_constructible_v<ui::ShaderProgram>);
static_assert(!std::is_move_constructible_v<ui::ShaderProgram>);
static_assert(std::is_nothrow_destructible_v<ui::ShaderProgram>);

void nativeui_header_compile_shader() {}
