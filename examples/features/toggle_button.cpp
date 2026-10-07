#include "example_support.hpp"
#include <nativeui/toggle_button.hpp>

namespace {
int self_test() {
  ui::State<bool> bold{false};
  ui::UI tree{ui::ToggleButton{"Bold", bold}};
  example::Platform platform;
  tree.resize({160.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Space), platform);
  if (bold.get())
    return example::fail("ToggleButton committed before release");
  auto release = example::key(ui::Key::Space);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  if (!bold.get())
    return example::fail("ToggleButton failed to commit on release");
  ui::HeadlessRenderer renderer{{160.0f, 40.0f}, 1.0f};
  return renderer.render(tree) ? 0
                               : example::fail("ToggleButton rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> bold{false};
  ui::UI tree{ui::ToggleButton{"Bold", bold}};
  return example::run_window(tree, "NativeUI / ToggleButton", {240.0f, 90.0f});
}
