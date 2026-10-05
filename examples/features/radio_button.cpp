#include "example_support.hpp"
#include <nativeui/radio_button.hpp>

namespace {
ui::UI make_ui(ui::State<std::string> &selected) {
  ui::RadioGroup<std::string> group{selected};
  return ui::UI{
      ui::Column{ui::RadioButton{group, std::string{"small"}, "Small"},
                 ui::RadioButton{group, std::string{"large"}, "Large"}}};
}
int self_test() {
  ui::State<std::string> selected{"small"};
  auto tree = make_ui(selected);
  example::Platform platform;
  tree.resize({200.0f, 90.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Down), platform);
  if (selected.get() != "large")
    return example::fail("Radio group did not select the next option");
  ui::HeadlessRenderer renderer{{200.0f, 90.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("RadioButton headless render failed");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> selected{"small"};
  auto tree = make_ui(selected);
  return example::run_window(tree, "NativeUI / RadioButton", {240.0f, 120.0f});
}
