#include "example_support.hpp"
#include <nativeui/context_menu.hpp>
namespace {
int self_test() {
  int primary{}, contextual{};
  ui::UI tree{
      ui::ContextMenu{ui::Button{"Document", [&] { ++primary; }}.spec(),
                      std::vector<ui::PopupMenuItem>{ui::PopupMenuItem::action(
                          "Copy", [&] { ++contextual; })}}};
  example::Platform platform;
  tree.resize({440, 300});
  tree.activate(platform);
  ui::InputEvent request;
  request.type = ui::InputType::ContextMenu;
  request.position = {35, 20};
  tree.dispatch(request, platform);
  if (primary != 0 || contextual != 0 || tree.overlay_entries().size() != 1)
    return example::fail("ContextMenu secondary request failed");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (primary != 0 || contextual != 1 || !tree.overlay_entries().empty())
    return example::fail("ContextMenu shared menu action failed");
  ui::HeadlessRenderer renderer{{440, 300}, 1};
  return renderer.render(tree) ? 0 : example::fail("ContextMenu render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<int> actions{0};
  auto count = actions.binding();
  ui::UI tree{ui::ContextMenu{
      ui::Label{"Secondary click, Menu or Shift+F10: document commands"}
          .spec(),
      std::vector<ui::PopupMenuItem>{
          ui::PopupMenuItem::action(
              "Copy", [count]() mutable { count.set(count.get() + 1); }),
          ui::PopupMenuItem::separator(),
          ui::PopupMenuItem::action("Unavailable", [] {}, false)}}};
  return example::run_window(tree, "NativeUI / ContextMenu", {620, 320});
}
