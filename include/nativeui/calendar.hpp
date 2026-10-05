#pragma once
#include <chrono>
#include <functional>
#include <nativeui/button.hpp>
#include <optional>
namespace ui {
struct CalendarStyle {
  std::optional<Color> background, text, muted_text, border, selected, cursor,
      today;
  double cell_width{36}, cell_height{32}, header_height{32}, weekday_height{24},
      padding{8}, gap{2}, text_size{14}, corner_radius{4};
  ButtonStyle navigation;
};
class Calendar {
public:
  using Value = std::optional<std::chrono::sys_days>;
  Calendar(std::string label, Binding<Value> selection);
  Calendar(std::string label, State<Value> &selection);
  Calendar &&range(Value minimum, Value maximum) &&;
  Calendar &&reference_day(std::chrono::sys_days value) &&;
  Calendar &&today(Value value) &&;
  Calendar &&commit_on_navigation(bool value = true) &&;
  Calendar &&on_change(std::function<void(Value)> callback) &&;
  Calendar &&style(CalendarStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<Value> value_;
  Value minimum_, maximum_, today_;
  std::chrono::sys_days reference_{};
  bool commit_{true};
  std::function<void(Value)> callback_;
  CalendarStyle style_;
};
} // namespace ui
