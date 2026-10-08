#include "test_support.hpp"
#include <nativeui/combo_box.hpp>
namespace {
struct Identity {
  int id{};
  std::string metadata;
  bool operator==(const Identity &) const = default;
};
void key_release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void suite() {
  ui::State<Identity> selected{Identity{99, "unknown"}};
  std::vector<ui::ComboBoxOption<Identity>> model{
      {{1, "one"}, "One", true},
      {{2, "two"}, "Two", false},
      {{3, "three"}, "Three", true}};
  int provider_calls{}, writes{};
  auto observation = selected.observe([&](const auto &) { ++writes; });
  ui::UI tree{ui::ComboBox<Identity>{
      selected, [&] {
        ++provider_calls;
        return model;
      }}.placeholder("Unknown")};
  test::MockPlatform platform;
  tree.resize({440, 280});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{440, 280}, 1};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(provider_calls == 1 && writes == 0 && selected.get().id == 99);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(provider_calls == 2 && writes == 0);
  tree.dispatch(test::key(ui::Key::Down), platform);
  key_release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(writes == 0);
  model = {{{8, "eight"}, "Eight", true}};
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get().id == 3 && writes == 1 && provider_calls == 2);
  key_release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Down), platform);
  key_release(tree, platform, ui::Key::Down);
  NUI_CHECK(provider_calls == 3);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(selected.get().id == 3 && writes == 1);
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(provider_calls == 3);
}
} // namespace
int main() { return test::run("widget_combo_box", &suite); }
