#define NATIVEUI_ENABLE_INSPECTOR 1

#include <nativeui/component.hpp>
#include <nativeui/component_tree.hpp>
#include <nativeui/inspector.hpp>

#include <type_traits>
#include <utility>

static_assert(std::is_same_v<
              decltype(std::declval<ui::Tree&>().inspector_snapshot()),
              ui::debug::InspectorSnapshot>);
static_assert(std::is_copy_constructible_v<ui::debug::InspectorSnapshot>);
static_assert(std::is_copy_constructible_v<ui::debug::InspectorNode>);

void nativeui_header_compile_component() {}
