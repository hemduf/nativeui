#include "example_support.hpp"
#include <nativeui/popup_menu.hpp>
namespace {
ui::PopupMenuItem submenu(std::function<void()> choose) {
  ui::PopupMenuItem item;
  item.key = "format";
  item.label = "Format";
  auto child = ui::PopupMenuItem::action("Compact", std::move(choose));
  child.key = "compact";
  child.checked = true;
  child.shortcut_label = "Ctrl+1";
  item.children.push_back(std::move(child));
  return item;
}
int self_test() {
  int calls{}, providers{};
  ui::UI tree{ui::PopupMenu{"Actions", [&] {
                              ++providers;
                              return std::vector<ui::PopupMenuItem>{
                                  ui::PopupMenuItem::separator(),
                                  submenu([&] { ++calls; })};
                            }}};
  example::Platform platform;
  tree.resize({420, 300});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Down), platform);
  auto up = example::key(ui::Key::Down);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(example::key(ui::Key::Right), platform);
  if (tree.overlay_entries().size() != 2 || providers != 1 || calls != 0)
    return example::fail("PopupMenu submenu snapshot failed");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (calls != 1 || !tree.overlay_entries().empty())
    return example::fail("PopupMenu close-before-action failed");
  ui::HeadlessRenderer renderer{{420, 300}, 1};
  return renderer.render(tree) ? 0 : example::fail("PopupMenu render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> compact{true};
  auto value = compact.binding();
  ui::UI tree{ui::PopupMenu{"Presentation", [value] {
                              auto item = submenu([value]() mutable {
                                value.set(!value.get());
                              });
                              item.children[0].checked = value.get();
                              return std::vector<ui::PopupMenuItem>{item};
                            }}};
  return example::run_window(tree, "NativeUI / PopupMenu", {440, 320});
}
