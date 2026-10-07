#include "test_support.hpp"
#include <nativeui/combo_popup.hpp>
namespace {
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void one_recipe_has_independent_compilation_runtime() {
  int calls{}, providers{};
  auto recipe =
      ui::PopupMenu{"Actions",
                    [&] {
                      ++providers;
                      return std::vector<ui::PopupMenuItem>{
                          ui::PopupMenuItem::action("Open", [&] { ++calls; }),
                          ui::PopupMenuItem::separator(),
                          ui::PopupMenuItem::action("Disabled", [] {}, false)};
                    }}
          .style(ui::ComboBoxStyle{})
          .item_style(ui::MenuItemStyle{})
          .spec();
  ui::UI left{ui::Spec{recipe}}, right{ui::Spec{recipe}};
  test::MockPlatform a, b;
  left.resize({320, 240});
  right.resize({320, 240});
  left.activate(a);
  right.activate(b);
  left.dispatch(test::key(ui::Key::Down), a);
  right.dispatch(test::key(ui::Key::Down), b);
  NUI_CHECK(providers == 2 && left.overlay_entries().size() == 1 &&
            right.overlay_entries().size() == 1);
  release(left, a, ui::Key::Down);
  left.dispatch(test::key(ui::Key::Enter), a);
  NUI_CHECK(calls == 1 && left.overlay_entries().empty() &&
            right.overlay_entries().size() == 1);
  release(right, b, ui::Key::Down);
  right.dispatch(test::key(ui::Key::Enter), b);
  NUI_CHECK(calls == 2 && right.overlay_entries().empty());
}
void empty_snapshot_opens_and_closes_without_an_action() {
  ui::UI tree{ui::PopupMenu{"Empty", std::vector<ui::PopupMenuItem>{}}};
  test::MockPlatform platform;
  tree.resize({320, 240});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(tree.overlay_entries().empty());
}
void suite() {
  one_recipe_has_independent_compilation_runtime();
  empty_snapshot_opens_and_closes_without_an_action();
}
} // namespace
int main() { return test::run("widget_popup_menu_legacy", &suite); }
