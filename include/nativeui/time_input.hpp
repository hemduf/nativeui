#pragma once
#include <chrono>
#include <functional>
#include <nativeui/button.hpp>
#include <optional>
namespace ui {
struct TimeInputStyle {
  std::optional<Color> background, text, border, selected, focus, separator;
  double text_size{14}, segment_width{38}, height{32}, padding{4}, gap{3},
      corner_radius{5}, border_width{1};
  ButtonStyle clear;
};
class TimeInput {
public:
  using Value = std::optional<std::chrono::seconds>;
  TimeInput(std::string label, Binding<Value> value);
  TimeInput(std::string label, State<Value> &value);
  TimeInput &&show_seconds(bool value = true) &&;
  TimeInput &&clearable(bool value = true) &&;
  TimeInput &&on_change(std::function<void(Value)> callback) &&;
  TimeInput &&style(TimeInputStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<Value> value_;
  bool seconds_{}, clearable_{true};
  std::function<void(Value)> callback_;
  TimeInputStyle style_;
};
} // namespace ui
