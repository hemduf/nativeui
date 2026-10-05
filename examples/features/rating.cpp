#include "example_support.hpp"
#include <nativeui/rating.hpp>

namespace {
int self_test() {
  ui::State<double> score{3.0};
  ui::UI tree{ui::Rating{"Note", score, 5}.step(0.5)};
  example::Platform platform;
  tree.resize({160.0f, 32.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Right), platform);
  if (score.get() != 3.5)
    return example::fail("Rating failed to increment by half");
  tree.dispatch(example::key(ui::Key::End), platform);
  if (score.get() != 5.0)
    return example::fail("Rating failed to clamp to maximum");
  ui::HeadlessRenderer renderer{{160.0f, 32.0f}, 1.0f};
  return renderer.render(tree) ? 0 : example::fail("Rating rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<double> score{3.0};
  ui::UI tree{ui::Rating{"Note", score, 5}.step(0.5)};
  return example::run_window(tree, "NativeUI / Rating", {240.0f, 90.0f});
}
