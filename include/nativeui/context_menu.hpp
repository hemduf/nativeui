#pragma once
#include <nativeui/popup_menu.hpp>
namespace ui {
class ContextMenu {
public:
  ContextMenu(Spec child, PopupMenu::ItemsProvider items);
  ContextMenu(Spec child, std::vector<PopupMenuItem> items);
  ContextMenu &&item_style(MenuItemStyle value) &&;
  Spec spec() &&;

private:
  Spec child_;
  PopupMenu::ItemsProvider provider_;
  MenuItemStyle item_style_;
};
} // namespace ui
