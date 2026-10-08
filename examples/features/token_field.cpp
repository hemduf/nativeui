#include "example_support.hpp"
#include <nativeui/token_field.hpp>
int main(int argc, char **argv) {
  ui::State<std::vector<std::string>> value{std::vector<std::string>{"Audio"}};
  ui::UI tree{ui::TokenField{"Tags", value, {"Audio", "Synthesis", "Effects"}}
                  .placeholder("Add a tag")};
  if (example::self_test_requested(argc, argv)) {
    example::Platform platform;
    tree.resize({360, 160});
    tree.activate(platform);
    ui::InputEvent event;
    event.type = ui::InputType::TextInput;
    event.text = "Synthesis,Effects,";
    tree.dispatch(event, platform);
    return value.get() ==
                   std::vector<std::string>({"Audio", "Synthesis", "Effects"})
               ? 0
               : example::fail("Token batch did not commit once");
  }
  return example::run_window(tree, "NativeUI / TokenField", {480, 260});
}
