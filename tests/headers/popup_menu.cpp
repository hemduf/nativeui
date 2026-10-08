#include <nativeui/popup_menu.hpp>
#include <type_traits>
static_assert(std::is_same_v<ui::PopupMenu::ItemsProvider,
                             std::function<std::vector<ui::PopupMenuItem>()>>);
void popup_menu_header_probe() {
  auto item = ui::PopupMenuItem::action("Open", [] {});
  item.key = "open";
  item.checked = false;
  item.shortcut_label = "Ctrl+O";
  item.children = {};
  auto recipe = ui::PopupMenu{"Actions", std::vector<ui::PopupMenuItem>{item}}
                    .style(ui::ComboBoxStyle{})
                    .item_style(ui::MenuItemStyle{})
                    .spec();
  (void)recipe;
}
