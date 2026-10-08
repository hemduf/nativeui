#pragma once
#include <functional>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/text_input.hpp>
#include <string_view>
namespace ui {
struct EditableComboBoxStyle {
  TextInputStyle text_input;
  ComboBoxStyle frame;
  MenuItemStyle item;
  float chevron_width{28.f};
  std::size_t maximum_visible_rows{8};
};
class EditableComboBox {
public:
  using OptionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view, std::string_view)>;
  EditableComboBox(std::string, Binding<std::string>, std::vector<std::string>);
  EditableComboBox(std::string, State<std::string> &, std::vector<std::string>);
  EditableComboBox(std::string, Binding<std::string>, OptionsProvider);
  EditableComboBox(std::string, State<std::string> &, OptionsProvider);
  EditableComboBox &&filter(Filter) &&;
  EditableComboBox &&placeholder(std::string) &&;
  EditableComboBox &&style(EditableComboBoxStyle) &&;
  Spec spec() &&;

private:
  std::string label_, placeholder_;
  Binding<std::string> selected_;
  OptionsProvider options_;
  Filter filter_;
  EditableComboBoxStyle style_;
};
} // namespace ui
