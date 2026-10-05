#include "example_support.hpp"
#include <nativeui/date_input.hpp>
int main(int argc, char **argv) {
  const auto initial = std::chrono::sys_days{std::chrono::year{2026} / 10 / 4};
  ui::State<ui::DateInput::Value> value{initial};
  ui::UI tree{ui::DateInput{"Due date", value}};
  if (example::self_test_requested(argc, argv)) {
    example::Platform platform;
    tree.resize({500, 450});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    tree.dispatch(example::key(ui::Key::Right), platform);
    tree.dispatch(example::key(ui::Key::Escape), platform);
    return value.get() == initial && tree.overlay_entries().empty()
               ? 0
               : example::fail("Date cancel changed the model");
  }
  return example::run_window(tree, "NativeUI / DateInput", {500, 450});
}
