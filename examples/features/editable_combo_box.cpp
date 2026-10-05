#include "example_support.hpp"
#include <nativeui/editable_combo_box.hpp>
namespace {
ui::InputEvent text(std::string value) {
  ui::InputEvent event;
  event.type = ui::InputType::TextInput;
  event.text = std::move(value);
  return event;
}
int self_test() {
  ui::State<std::string> selected{"Inter"};
  ui::UI tree{ui::EditableComboBox{
      "Police", selected,
      std::vector<std::string>{"Inter", "Georgia", "Menlo"}}};
  example::Platform platform;
  tree.resize({440, 300});
  tree.activate(platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(text("ge"), platform);
  if (selected.get() != "Inter")
    return example::fail("Draft changed selected font");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  return selected.get() == "Georgia"
             ? 0
             : example::fail("EditableComboBox did not choose match");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> selected{"Inter"};
  ui::UI tree{ui::EditableComboBox{
      "Police", selected,
      std::vector<std::string>{"Inter", "Georgia", "Menlo"}}};
  return example::run_window(tree, "NativeUI / EditableComboBox", {460, 360});
}
