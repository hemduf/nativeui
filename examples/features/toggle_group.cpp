#include "example_support.hpp"
#include <nativeui/toggle_button.hpp>
#include <nativeui/toggle_group.hpp>
namespace {
ui::Spec group(ui::State<bool> &bold, ui::State<bool> &italic,
               ui::State<bool> &underline) {
  std::vector<ui::Spec> items;
  items.push_back(ui::ToggleButton{"Bold", bold}.spec());
  items.push_back(ui::ToggleButton{"Italic", italic}.spec());
  items.push_back(ui::ToggleButton{"Underline", underline}.spec());
  return ui::ToggleGroup{"Text style", std::move(items)}.spec();
}
int self_test() {
  ui::State<bool> bold{false}, italic{false}, underline{false};
  ui::UI tree{group(bold, italic, underline)};
  example::Platform platform;
  tree.resize({400, 70});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Right), platform);
  if (bold.get() || italic.get() || underline.get())
    return example::fail("ToggleGroup navigation changed a value");
  tree.dispatch(example::key(ui::Key::Space), platform);
  auto up = example::key(ui::Key::Space);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  if (bold.get() || !italic.get() || underline.get())
    return example::fail("ToggleGroup activation did not remain independent");
  ui::HeadlessRenderer renderer{{400, 70}, 1};
  return renderer.render(tree) ? 0 : example::fail("ToggleGroup render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> bold{false}, italic{false}, underline{false};
  ui::UI tree{group(bold, italic, underline)};
  return example::run_window(tree, "NativeUI / ToggleGroup", {440, 90});
}
