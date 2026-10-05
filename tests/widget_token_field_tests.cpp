#include "test_support.hpp"
#include <nativeui/token_field.hpp>
namespace {
ui::InputEvent composition(ui::CompositionType type, std::string text = {}) {
  ui::InputEvent event;
  event.type = ui::InputType::Composition;
  event.composition.type = type;
  event.composition.text = std::move(text);
  return event;
}
void active_composition_does_not_submit_the_committed_draft() {
  ui::State<std::vector<std::string>> tokens{std::vector<std::string>{}};
  int publications{};
  auto observer = tokens.observe([&](const auto &) { ++publications; });
  ui::UI tree{ui::TokenField{"Tags", tokens}};
  test::MockPlatform platform;
  tree.resize({360, 150});
  tree.activate(platform);
  tree.dispatch(test::text("draft"), platform);
  tree.dispatch(composition(ui::CompositionType::Start), platform);
  tree.dispatch(composition(ui::CompositionType::Update, "!"), platform);
  for (const auto key : {ui::Key::Enter, ui::Key::Up, ui::Key::Down,
                         ui::Key::Left, ui::Key::Right, ui::Key::Backspace,
                         ui::Key::Delete})
    tree.dispatch(test::key(key), platform);
  NUI_CHECK(tokens.get().empty() && publications == 0);
  tree.dispatch(composition(ui::CompositionType::Commit, "!"), platform);
  tree.dispatch(test::text("!"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>{"draft!"} &&
            publications == 1);
  tree.dispatch(test::text("next"), platform);
  tree.dispatch(composition(ui::CompositionType::Start), platform);
  tree.dispatch(composition(ui::CompositionType::Update, "-preedit"), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>({"draft!", "next"}) &&
            publications == 2);
}
void active_composition_does_not_remove_a_previous_token() {
  ui::State<std::vector<std::string>> tokens{std::vector<std::string>{"old"}};
  ui::UI tree{ui::TokenField{"Tags", tokens}};
  test::MockPlatform platform;
  tree.resize({360, 150});
  tree.activate(platform);
  tree.dispatch(composition(ui::CompositionType::Start), platform);
  tree.dispatch(composition(ui::CompositionType::Update, "new,"), platform);
  tree.dispatch(test::key(ui::Key::Backspace), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>{"old"});
  tree.dispatch(composition(ui::CompositionType::Commit, "new,"), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>({"old", "new"}));
}
void suite() {
  active_composition_does_not_submit_the_committed_draft();
  active_composition_does_not_remove_a_previous_token();
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
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "composition_submit")
    return test::run("token_composition_submit",
                     &active_composition_does_not_submit_the_committed_draft);
  if (argc == 2 && std::string_view{argv[1]} == "composition_backspace")
    return test::run("token_composition_backspace",
                     &active_composition_does_not_remove_a_previous_token);
  return test::run("widget_token_field", &suite);
}
