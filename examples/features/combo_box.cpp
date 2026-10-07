#include "example_support.hpp"
#include <nativeui/combo_box.hpp>
namespace {
int self_test() {
  ui::State<int> selected{99};
  ui::UI tree{
      ui::ComboBox<int>{selected, {{1, "Normal", true}, {2, "High", true}}}};
  example::Platform platform;
  tree.resize({360, 220});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Down), platform);
  auto release = example::key(ui::Key::Down);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  tree.dispatch(example::key(ui::Key::End), platform);
  if (selected.get() != 99)
    return example::fail("Preview changed selection");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  return selected.get() == 2
             ? 0
             : example::fail("ComboBox did not choose snapshot");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<int> selected{1};
  ui::UI tree{
      ui::ComboBox<int>{selected, {{1, "Normal", true}, {2, "High", true}}}};
  return example::run_window(tree, "NativeUI / ComboBox", {420, 320});
}
