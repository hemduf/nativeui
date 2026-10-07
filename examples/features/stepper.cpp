#include "example_support.hpp"
#include <nativeui/stepper.hpp>

namespace {
int self_test() {
  ui::State<double> copies{1.0};
  ui::UI tree{ui::Stepper{copies}.label("Copies").range(1.0, 99.0)};
  example::Platform platform;
  tree.resize({24.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Up), platform);
  if (copies.get() != 2.0)
    return example::fail("Stepper failed to increment");
  tree.dispatch(example::key(ui::Key::Home), platform);
  if (copies.get() != 1.0)
    return example::fail("Stepper failed to clamp to minimum");
  ui::HeadlessRenderer renderer{{24.0f, 40.0f}, 1.0f};
  return renderer.render(tree) ? 0 : example::fail("Stepper rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<double> copies{1.0};
  ui::UI tree{ui::Stepper{copies}.label("Copies").range(1.0, 99.0)};
  return example::run_window(tree, "NativeUI / Stepper", {120.0f, 90.0f});
}
