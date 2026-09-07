#include <nativeui/window.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_copy_assignable_v<ui::StandaloneWindow>);
static_assert(!std::is_move_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_move_assignable_v<ui::StandaloneWindow>);

static_assert(!std::is_copy_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_copy_assignable_v<ui::EmbeddedView>);
static_assert(!std::is_move_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_move_assignable_v<ui::EmbeddedView>);

void nativeui_header_compile_window() {}
