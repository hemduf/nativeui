#include <nativeui/button.hpp>
#include <nativeui/toolbar.hpp>
#include <type_traits>
static_assert(std::is_constructible_v<ui::Toolbar, std::string,
                                      std::vector<ui::ToolbarItem>>);
ui::Spec toolbar_header_probe() {
  ui::ToolbarStyle style;
  style.overflow_label = "More";
  std::vector<ui::ToolbarItem> items;
  items.push_back({"save", ui::Button{"Save", [] {}}.spec(),
                   ui::PopupMenuItem::action("Save", [] {})});
  return ui::Toolbar{"Document", std::move(items)}
      .style(std::move(style))
      .spec();
}
