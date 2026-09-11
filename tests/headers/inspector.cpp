#include <nativeui/inspector.hpp>
#include <nativeui/ui.hpp>

#include <type_traits>
#include <utility>

static_assert(std::is_copy_constructible_v<ui::debug::InspectorSnapshot>);
static_assert(std::is_copy_constructible_v<ui::debug::InspectorNode>);

#if defined(NATIVEUI_ENABLE_INSPECTOR)
static_assert(std::is_same_v<
              decltype(ui::debug::inspector_snapshot(std::declval<ui::UI&>())),
              ui::debug::InspectorSnapshot>);
static_assert(std::is_same_v<
              decltype(ui::debug::inspector_enabled(std::declval<const ui::UI&>())),
              bool>);
#endif

void nativeui_header_compile_inspector() {}
