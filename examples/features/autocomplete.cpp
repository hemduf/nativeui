#include "example_support.hpp"
#include <nativeui/autocomplete.hpp>
namespace {
ui::InputEvent text(std::string value) {
  ui::InputEvent event;
  event.type = ui::InputType::TextInput;
  event.text = std::move(value);
  return event;
}
int self_test() {
  ui::State<std::string> value{""};
  int submits{};
  ui::UI tree{ui::Autocomplete{"City", value,
                               std::vector<std::string>{"Paris", "Pau", "Lyon"}}
                  .on_submit([&](const auto &) { ++submits; })};
  example::Platform platform;
  tree.resize({440, 300});
  tree.activate(platform);
  tree.dispatch(text("pa"), platform);
  tree.dispatch(example::key(ui::Key::Down), platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  return value.get() == "Paris" && submits == 0
             ? 0
             : example::fail("Autocomplete did not choose without submit");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> value{""};
  ui::UI tree{ui::Autocomplete{"City", value,
                               std::vector<std::string>{"Paris", "Pau", "Lyon"}}
                  .placeholder("Free text or suggestion")};
  return example::run_window(tree, "NativeUI / Autocomplete", {460, 360});
}
