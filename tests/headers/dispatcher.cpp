#include <nativeui/dispatcher.hpp>

#include <type_traits>

static_assert(std::is_copy_constructible_v<ui::Dispatcher>);
static_assert(std::is_copy_assignable_v<ui::Dispatcher>);
static_assert(std::is_default_constructible_v<ui::TimerHandle>);
static_assert(ui::kDispatcherMaxPendingTasks == 65'536);
static_assert(ui::kDispatcherMaxActiveTimers == 8'192);
static_assert(ui::kDispatcherMaxTasksPerCheckpoint == 1'024);
