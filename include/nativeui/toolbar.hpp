#pragma once
#include <nativeui/component_base.hpp>
#include <nativeui/popup_menu.hpp>
#include <nativeui/style.hpp>
#include <optional>
#include <string>
#include <vector>
namespace ui {
struct ToolbarItem {
  std::string key;
  Spec content;
  std::optional<PopupMenuItem> overflow_item;
};
struct ToolbarStyle {
  float padding{4}, gap{4}, minimum_height{40};
  std::string overflow_label{"More"};
  ButtonStyle controls;
  ComboBoxStyle overflow_button;
  MenuItemStyle overflow_items;
};
class Toolbar {
public:
  Toolbar(std::string label, std::vector<ToolbarItem> items);
  Toolbar &&style(ToolbarStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  std::vector<ToolbarItem> items_;
  ToolbarStyle style_;
};
} // namespace ui
