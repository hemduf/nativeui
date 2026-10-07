#include "example_support.hpp"
#include <nativeui/spinner.hpp>

namespace {
ui::UI make_ui(ui::State<bool> &working) {
  return ui::UI{ui::Spinner{"Loading"}.active(working).size(24.0)};
}
int self_test() {
  ui::State<bool> working{true};
  auto tree = make_ui(working);
  ui::HeadlessRenderer renderer{{40.0f, 40.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("Spinner headless render failed");
  const auto active = renderer.rgba_pixels();
  working.set(false);
  if (!renderer.render(tree))
    return example::fail("Spinner inactive render failed");
  if (active == renderer.rgba_pixels())
    return example::fail("Spinner remained visible when inactive");
  if (!example::near(tree.measure().preferred.w, 24.0f))
    return example::fail("Spinner activity changed its measured size");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> working{true};
  auto tree = make_ui(working);
  return example::run_window(tree, "NativeUI / Spinner", {120.0f, 120.0f});
}
