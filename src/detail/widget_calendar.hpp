#pragma once
#include <nativeui/calendar.hpp>
namespace ui::detail {
struct CalendarConfig {
  Calendar::Value minimum, maximum, today;
  std::chrono::sys_days reference{};
  bool commit_navigation{true};
  CalendarStyle style;
  std::function<void(Calendar::Value)> changed;
  // An owning compound control may close its popup even for an unchanged day.
  // This is called only from a validated user activation, never observation.
  std::function<void(std::chrono::sys_days)> picked;
  Key suppressed{Key::None};
};
void validate_calendar(const CalendarConfig &);
bool calendar_day_valid(std::chrono::sys_days) noexcept;
std::string calendar_iso(Calendar::Value);
Spec calendar_spec(std::string label, Binding<Calendar::Value>, CalendarConfig);
} // namespace ui::detail
