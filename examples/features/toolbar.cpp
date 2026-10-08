#include "example_support.hpp"
#include <nativeui/button.hpp>
#include <nativeui/toolbar.hpp>
namespace {
ui::Spec bar(int &calls) {
  ui::ButtonStyle button;
  button.base.minimum_width = 60.0f;
  button.base.horizontal_padding = 0.0f;
  button.base.text_size = 1.0f;
  ui::ToolbarStyle style;
  style.padding = 0;
  style.gap = 0;
  style.overflow_button.base.minimum_width = 48.0f;
  style.overflow_button.base.horizontal_padding = 0.0f;
  style.overflow_button.base.text_size = 1.0f;
  std::vector<ui::ToolbarItem> items;
  for (const std::string label : {"Save", "Undo", "Redo"}) {
    auto action = [&calls] { ++calls; };
    items.push_back({label, ui::Button{label, action}.style(button).spec(),
                     ui::PopupMenuItem::action(label, action)});
  }
  return ui::Toolbar{"Document", std::move(items)}.style(style).spec();
}
int self_test() {
  int calls{};
  ui::UI tree{bar(calls)};
  example::Platform platform;
  tree.resize({120, 60});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::End), platform);
  tree.dispatch(example::key(ui::Key::Down), platform);
  auto up = example::key(ui::Key::Down);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  if (tree.overlay_entries().size() != 1)
    return example::fail("Toolbar overflow menu did not open");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (calls != 1 || !tree.overlay_entries().empty())
    return example::fail(
        "Toolbar overflow command did not close before invocation");
  tree.resize({320, 60});
  ui::HeadlessRenderer renderer{{320, 60}, 1};
  return renderer.render(tree) ? 0
                               : example::fail("Toolbar resize render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  int calls{};
  ui::UI tree{bar(calls)};
  return example::run_window(tree, "NativeUI / Toolbar", {320, 90});
}
