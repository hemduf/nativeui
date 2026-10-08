#include "example_support.hpp"
#include <nativeui/history_button.hpp>
namespace {
int self_test() {
  ui::State<bool> can{true};
  ui::State<std::vector<ui::HistoryEntry>> entries{
      std::vector<ui::HistoryEntry>{
          {"one", "First"}, {"two", "Second"}, {"three", "Third"}}};
  int chosen{};
  ui::UI tree{ui::HistoryButton{
      ui::HistoryDirection::Backward, can, [&](int steps) {
        chosen = steps;
      }}.entries(entries)};
  example::Platform platform;
  tree.resize({420, 300});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Space), platform);
  auto up = example::key(ui::Key::Space);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  if (chosen != -1)
    return example::fail("HistoryButton primary navigation failed");
  ui::InputEvent menu;
  menu.type = ui::InputType::ContextMenu;
  menu.position = {10, 15};
  tree.dispatch(menu, platform);
  tree.dispatch(example::key(ui::Key::End), platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (chosen != -3 || !tree.overlay_entries().empty())
    return example::fail("HistoryButton menu navigation failed");
  ui::HeadlessRenderer renderer{{420, 300}, 1};
  return renderer.render(tree)
             ? 0
             : example::fail("HistoryButton rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> can{true};
  ui::State<std::vector<ui::HistoryEntry>> entries{
      std::vector<ui::HistoryEntry>{
          {"editor", "Editor"}, {"presets", "Presets"}, {"home", "Home"}}};
  auto available = can.binding();
  ui::HistoryButtonStyle style;
  style.show_label = true;
  ui::UI tree{
      ui::HistoryButton{ui::HistoryDirection::Backward, can,
                        [available](int) mutable { available.set(false); }}
          .entries(entries)
          .style(style)};
  return example::run_window(tree, "NativeUI / HistoryButton", {260, 90});
}
