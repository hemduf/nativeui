#pragma once
#include <nativeui/calendar.hpp>
namespace ui {
struct DateInputStyle {
  std::optional<Color> background, text, placeholder, border, focus;
  double minimum_width{170}, height{32}, padding{8}, text_size{14},
      corner_radius{5}, border_width{1};
  ButtonStyle clear;
  CalendarStyle calendar;
};
class DateInput {
public:
  using Value = Calendar::Value;
  DateInput(std::string label, Binding<Value> value);
  DateInput(std::string label, State<Value> &value);
  DateInput &&range(Value minimum, Value maximum) &&;
  DateInput &&reference_day(std::chrono::sys_days value) &&;
  DateInput &&placeholder(std::string value) &&;
  DateInput &&clearable(bool value = true) &&;
  DateInput &&on_change(std::function<void(Value)> callback) &&;
  DateInput &&style(DateInputStyle value) &&;
  Spec spec() &&;

private:
  std::string label_, placeholder_{"Choose a date"};
  Binding<Value> value_;
  Value minimum_, maximum_;
  std::chrono::sys_days reference_{};
  bool clearable_{true};
  std::function<void(Value)> callback_;
  DateInputStyle style_;
};
} // namespace ui
