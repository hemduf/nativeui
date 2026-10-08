#pragma once
#include <nativeui/checkbox.hpp>
#include <optional>

namespace ui {
struct CheckboxGroupItem {
  std::string key;
  std::string label;
  Binding<bool> checked;
  bool enabled{true};
  bool read_only{};
};
struct CheckboxGroupStyle {
  CheckboxStyle parent;
  CheckboxStyle child;
  float indentation{20};
  float gap{4};
  std::optional<Color> mixed_color;
};
class CheckboxGroup {
public:
  CheckboxGroup(std::string label, std::vector<CheckboxGroupItem> items);
  CheckboxGroup&& style(CheckboxGroupStyle value) &&;
  Spec spec() &&;
private:
  std::string label_;
  std::vector<CheckboxGroupItem> items_;
  CheckboxGroupStyle style_;
};
} // namespace ui
