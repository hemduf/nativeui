#include "example_support.hpp"
#include <nativeui/link.hpp>

namespace {
int self_test() {
  std::string destination;
  ui::UI tree{ui::Link{"Help", "/help",
                       [&](const std::string &value) { destination = value; }}};
  example::Platform platform;
  tree.resize({120, 40});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (destination != "/help")
    return example::fail("Link did not deliver its destination");
  ui::HeadlessRenderer renderer{{120, 40}, 1.0f};
  return renderer.render(tree) ? 0 : example::fail("Link rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::UI tree{ui::Link{"Help", "/help", [](const std::string &) {}}};
  return example::run_window(tree, "NativeUI / Link", {240, 90});
}
