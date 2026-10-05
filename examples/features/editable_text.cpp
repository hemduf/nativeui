#include "example_support.hpp"
#include <nativeui/editable_text.hpp>
namespace {
int self_test() {
  ui::State<std::string> name{"Preset.oreto"};
  ui::UI tree{ui::EditableText{"Name", name}.select_stem()};
  example::Platform platform;
  tree.resize({420, 80});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::F2), platform);
  ui::InputEvent text;
  text.type = ui::InputType::TextInput;
  text.text = "Edited";
  tree.dispatch(text, platform);
  if (name.get() != "Preset.oreto")
    return example::fail("EditableText published its private draft");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (name.get() != "Edited.oreto")
    return example::fail("EditableText did not commit its selected stem");
  ui::HeadlessRenderer renderer{{420, 80}, 1};
  return renderer.render(tree) ? 0
                               : example::fail("EditableText rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> name{"Preset.oreto"};
  ui::UI tree{ui::EditableText{"Preset name", name}.select_stem()};
  return example::run_window(tree, "NativeUI / EditableText", {420, 110});
}
