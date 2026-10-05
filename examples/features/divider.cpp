#include "example_support.hpp"
#include <nativeui/divider.hpp>

namespace {
ui::UI make_ui() {
  return ui::UI{ui::Divider{}.thickness(2.0).color({0.8f, 0.2f, 0.1f, 1.0f})};
}
int self_test() {
  auto tree = make_ui();
  ui::HeadlessRenderer renderer{{120.0f, 24.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("Divider headless render failed");
  if (!example::near(tree.measure().preferred.h, 2.0f))
    return example::fail("Divider thickness differs from its measured size");
  const auto line = renderer.pixel(30, 0);
  if (line.r <= line.g)
    return example::fail("Divider did not use the explicit color");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  auto tree = make_ui();
  return example::run_window(tree, "NativeUI / Divider", {420.0f, 120.0f});
}
