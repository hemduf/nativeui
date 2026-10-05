#include "example_support.hpp"
#include <nativeui/time_input.hpp>
int main(int argc, char **argv) {
  ui::State<ui::TimeInput::Value> value{
      std::chrono::seconds{9 * 3600 + 30 * 60 + 17}};
  ui::UI tree{ui::TimeInput{"Alarme", value}};
  if (example::self_test_requested(argc, argv)) {
    example::Platform platform;
    tree.resize({320, 80});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Up), platform);
    return value.get() == std::chrono::seconds{10 * 3600 + 30 * 60 + 17}
               ? 0
               : example::fail("Hidden seconds were not preserved");
  }
  return example::run_window(tree, "NativeUI / TimeInput", {360, 120});
}
