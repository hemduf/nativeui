#include <nativeui/desktop_services.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<ui::DesktopServices>);
static_assert(!std::is_copy_assignable_v<ui::DesktopServices>);
static_assert(!std::is_move_constructible_v<ui::DesktopServices>);
static_assert(ui::kDesktopServicesMaxActiveFileChoosers == 1);
static_assert(ui::kDesktopServicesMaxActiveUrls == 16);
static_assert(ui::kInvalidDesktopRequestId == 0);
