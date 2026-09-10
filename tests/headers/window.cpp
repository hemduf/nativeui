#include <nativeui/window.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<ui::Application>);
static_assert(!std::is_copy_assignable_v<ui::Application>);
static_assert(!std::is_move_constructible_v<ui::Application>);
static_assert(!std::is_move_assignable_v<ui::Application>);
static_assert(std::is_same_v<decltype(std::declval<const ui::Application&>().valid()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ui::Application&>().last_error()), std::string_view>);
static_assert(std::is_same_v<decltype(std::declval<ui::Application&>().poll(0.0)), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::Application&>().run()), int>);
static_assert(std::is_same_v<decltype(std::declval<ui::Application&>().quit_requested()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ui::Application&>().quit_policy()), ui::QuitPolicy>);

static_assert(!std::is_copy_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_copy_assignable_v<ui::StandaloneWindow>);
static_assert(!std::is_move_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_move_assignable_v<ui::StandaloneWindow>);
static_assert(std::is_constructible_v<ui::StandaloneWindow, ui::Application&, ui::UI&, ui::WindowDesc>);
static_assert(std::is_same_v<decltype(std::declval<const ui::StandaloneWindow&>().valid()), bool>);

static_assert(!std::is_copy_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_copy_assignable_v<ui::EmbeddedView>);
static_assert(!std::is_move_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_move_assignable_v<ui::EmbeddedView>);

void nativeui_header_compile_window() {}
