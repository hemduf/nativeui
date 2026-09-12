#include <nativeui/window.hpp>

#include <functional>
#include <memory>
#include <optional>
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

static_assert(std::is_same_v<decltype(ui::WindowDesc{}.min_size), std::optional<ui::Size>>);
static_assert(std::is_same_v<decltype(ui::WindowDesc{}.max_size), std::optional<ui::Size>>);
static_assert(std::is_enum_v<ui::CloseDecision>);

static_assert(!std::is_copy_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_copy_assignable_v<ui::StandaloneWindow>);
static_assert(!std::is_move_constructible_v<ui::StandaloneWindow>);
static_assert(!std::is_move_assignable_v<ui::StandaloneWindow>);
static_assert(std::is_constructible_v<ui::StandaloneWindow, ui::Application&, ui::UI&, ui::WindowDesc>);
static_assert(std::is_same_v<decltype(std::declval<const ui::StandaloneWindow&>().valid()), bool>);
static_assert(std::is_same_v<
              decltype(std::declval<ui::StandaloneWindow&>().desktop_services()),
              ui::DesktopServices&>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().set_title(std::string_view{})), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().show()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().hide()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().set_min_size(std::optional<ui::Size>{})), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().set_max_size(std::optional<ui::Size>{})), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ui::StandaloneWindow&>().is_closed()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().on_close_request(
                                  std::function<ui::CloseDecision()>{})), void>);
static_assert(std::is_same_v<decltype(std::declval<ui::StandaloneWindow&>().on_closed(
                                  std::function<void()>{})), void>);

static_assert(!std::is_copy_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_copy_assignable_v<ui::EmbeddedView>);
static_assert(!std::is_move_constructible_v<ui::EmbeddedView>);
static_assert(!std::is_move_assignable_v<ui::EmbeddedView>);
static_assert(std::is_constructible_v<
              ui::EmbeddedView,
              ui::UI&,
              ui::NativeParentHandle,
              ui::Size,
              std::shared_ptr<ui::DesktopServicesBackend>>);
static_assert(std::is_same_v<
              decltype(std::declval<ui::EmbeddedView&>().desktop_services()),
              ui::DesktopServices&>);

void nativeui_header_compile_window() {}
