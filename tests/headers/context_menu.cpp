#include <nativeui/context_menu.hpp>
void context_menu_header_probe(ui::Spec child) {
  auto recipe = ui::ContextMenu{std::move(child), ui::PopupMenu::ItemsProvider{}}
                    .item_style(ui::MenuItemStyle{})
                    .spec();
  (void)recipe;
}
