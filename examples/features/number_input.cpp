#include "example_support.hpp"
#include <nativeui/number_input.hpp>
namespace {
int self_test() {
  ui::State<double> value{1};
  int submissions{};
  ui::UI tree{ui::NumberInput{"Copies", value}.range(0, 99).on_submit(
      [&](double) { ++submissions; })};
  example::Platform platform;
  tree.resize({460, 82});
  tree.activate(platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  ui::InputEvent text;
  text.type = ui::InputType::TextInput;
  text.text = "1.25";
  tree.dispatch(text, platform);
  if (value.get() != 1.25)
    return example::fail("NumberInput did not publish a valid draft");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (submissions != 1)
    return example::fail("NumberInput did not submit");
  text.text = "-";
  tree.dispatch(all, platform);
  tree.dispatch(text, platform);
  if (value.get() != 1.25)
    return example::fail("NumberInput published an incomplete draft");
  ui::HeadlessRenderer renderer{{460, 82}, 1};
  return renderer.render(tree) ? 0
                               : example::fail("NumberInput rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<double> value{1};
  ui::UI tree{
      ui::NumberInput{"Copies", value}.range(0, 99).step(0.25).precision(2)};
  return example::run_window(tree, "NativeUI / NumberInput", {460, 110});
}
