#include "test_support.hpp"
#include <nativeui/editable_combo_box.hpp>
namespace {
void replace(ui::UI &tree, test::MockPlatform &platform, std::string value) {
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  if (value.empty())
    tree.dispatch(test::key(ui::Key::Backspace), platform);
  else
    tree.dispatch(test::text(std::move(value)), platform);
}
void suite() {
  ui::State<std::string> compact_value{"Inter"};
  ui::UI compact{ui::EditableComboBox{
      "", compact_value, std::vector<std::string>{"Inter", "Menlo"}}};
  NUI_CHECK(compact.measure().preferred.h <= 50.0f);
  ui::State<std::string> selected{"Unknown"};
  int calls{}, writes{};
  auto observation = selected.observe([&](const auto &) { ++writes; });
  ui::UI tree{ui::EditableComboBox{"Choice", selected, [&] {
                                     ++calls;
                                     return std::vector<std::string>{
                                         "Japan", "Paris", "Pau", "Paris", ""};
                                   }}};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  replace(tree, platform, "pa");
  NUI_CHECK(selected.get() == "Unknown" && writes == 0 && calls == 1);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get() == "Paris" && writes == 1);
  NUI_CHECK(tree.overlay_entries().empty());
  auto up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  replace(tree, platform, "no match");
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get() == "Paris" && writes == 1);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(selected.get() == "Paris" && tree.overlay_entries().empty());
  replace(tree, platform, "pa");
  selected.set("external");
  ui::HeadlessRenderer recovery{{480, 350}, 1};
  NUI_CHECK(recovery.render(tree));
  NUI_CHECK(tree.overlay_entries().empty());
  NUI_CHECK(selected.get() == "external");
  ui::HeadlessRenderer renderer{{480, 350}, 1};
  NUI_CHECK(renderer.render(tree));
}
} // namespace
int main() { return test::run("widget_editable_combo_box", &suite); }
