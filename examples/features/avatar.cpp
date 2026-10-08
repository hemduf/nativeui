#include "example_support.hpp"
#include <nativeui/avatar.hpp>

namespace {
int self_test() {
  ui::State<std::string> name{"Camille Martin"};
  ui::UI tree{ui::Avatar{name}.size(32.0)};
  if (tree.measure().preferred.w != 32.0f)
    return example::fail("Avatar diameter was not measured");
  ui::HeadlessRenderer renderer{{64.0f, 40.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("Avatar rendering failed");
  const auto before = renderer.pixel(32, 7);
  name.set("Élodie Martin");
  if (!renderer.render(tree))
    return example::fail("Avatar Unicode update failed");
  const auto after = renderer.pixel(32, 7);
  if (before.r == after.r && before.g == after.g && before.b == after.b)
    return example::fail("Avatar name did not update its background");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::UI tree{ui::Avatar{"Camille Martin"}.size(48.0)};
  return example::run_window(tree, "NativeUI / Avatar", {120, 90});
}
