#include "example_support.hpp"
#include <nativeui/badge.hpp>

namespace {
ui::UI make_ui(ui::State<std::string> &count) {
  return ui::UI{ui::Badge{count}};
}
int self_test() {
  ui::State<std::string> count{"3"};
  auto tree = make_ui(count);
  ui::HeadlessRenderer renderer{{120.0f, 32.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("Badge headless render failed");
  const float initial_width = tree.measure().preferred.w;
  count.set("99+");
  if (!renderer.render(tree))
    return example::fail("Badge updated render failed");
  if (tree.measure().preferred.w <= initial_width)
    return example::fail("Badge did not update the text measurement");
  if (count.get() != "99+")
    return example::fail("Badge changed its model");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> count{"12"};
  auto tree = make_ui(count);
  return example::run_window(tree, "NativeUI / Badge", {180.0f, 48.0f});
}
