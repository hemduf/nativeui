#include "example_support.hpp"
#include <nativeui/calendar.hpp>
int main(int argc, char **argv) {
  const auto initial = std::chrono::sys_days{std::chrono::year{2024} / 1 / 31};
  ui::State<ui::Calendar::Value> value{initial};
  ui::UI tree{ui::Calendar{"Day", value}};
  if (example::self_test_requested(argc, argv)) {
    example::Platform platform;
    tree.resize({320, 300});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::PageDown), platform);
    return value.get() ==
                   std::chrono::sys_days{std::chrono::year{2024} / 2 / 29}
               ? 0
               : example::fail("Leap month navigation failed");
  }
  return example::run_window(tree, "NativeUI / Calendar", {360, 350});
}
