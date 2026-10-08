#include <nativeui/toggle_button.hpp>
#include <nativeui/toggle_group.hpp>
#include <type_traits>
static_assert(std::is_constructible_v<ui::ToggleGroup, std::string,
                                      std::vector<ui::Spec>>);
ui::Spec toggle_group_header_probe(ui::State<bool> &value) {
  ui::ToggleGroupStyle style;
  style.segment.base.minimum_width = 42.0f;
  style.selected.fill = ui::colors::accent;
  std::vector<ui::Spec> items;
  items.push_back(ui::ToggleButton{"Bold", value}.spec());
  return ui::ToggleGroup{"Style", std::move(items)}
      .style(std::move(style))
      .spec();
}
