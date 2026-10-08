#include "test_support.hpp"
#include <nativeui/autocomplete.hpp>
namespace {
void release(ui::UI &tree, test::MockPlatform &platform) {
  auto up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
}
void suite() {
  ui::State<std::string> value{""};
  int calls{}, submits{};
  std::string submitted;
  ui::UI tree{ui::Autocomplete{
      "City", value, [&] {
        ++calls;
        return std::vector<std::string>{"Japan", "Paris", "Pau", "Paris"};
      }}.on_submit([&](const auto &text) {
    ++submits;
    submitted = text;
  })};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  tree.dispatch(test::text("pa"), platform);
  NUI_CHECK(value.get() == "pa" && calls == 1);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "pa" && submits == 1 && submitted == "pa");
  release(tree, platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "Paris" && submits == 1);
  release(tree, platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("freeform"), platform);
  NUI_CHECK(value.get() == "freeform" && tree.overlay_entries().empty());
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 2 && submitted == "freeform");
  release(tree, platform);
  tree.dispatch(all, platform);
  tree.dispatch(test::text("pa"), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == "pa" && tree.overlay_entries().empty());
  ui::HeadlessRenderer renderer{{480, 350}, 1};
  NUI_CHECK(renderer.render(tree));
}
} // namespace
int main() { return test::run("widget_autocomplete", &suite); }
