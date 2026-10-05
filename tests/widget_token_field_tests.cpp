#include "test_support.hpp"
#include <nativeui/token_field.hpp>
namespace {
void suite() {
  ui::State<std::vector<std::string>> tokens{std::vector<std::string>{"Audio"}};
  int publications{};
  auto observer = tokens.observe([&](const auto &) { ++publications; });
  ui::UI tree{ui::TokenField{"Tags", tokens, {"Audio", "Effects", "Synth"}}};
  test::MockPlatform platform;
  tree.resize({360, 150});
  tree.activate(platform);
  tree.dispatch(test::text(" Effects, Audio, Synth,draft"), platform);
  NUI_CHECK(tokens.get() ==
                std::vector<std::string>({"Audio", "Effects", "Synth"}) &&
            publications == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tokens.get().back() == "draft" && publications == 2);
  tree.dispatch(test::key(ui::Key::Backspace), platform);
  NUI_CHECK(tokens.get().size() == 3 && publications == 3);
  tree.dispatch(test::key(ui::Key::Left), platform);
  tree.dispatch(test::key(ui::Key::Delete), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>({"Audio", "Effects"}));
  tokens.set({"x", "x", ""});
  ui::HeadlessRenderer renderer{{360, 150}, 1};
  NUI_CHECK(renderer.render(tree) && tokens.get().size() == 3);
}
} // namespace
int main() { return test::run("widget_token_field", &suite); }
