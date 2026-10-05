#include "test_support.hpp"
#include <nativeui/search_field.hpp>
namespace {
void escapes_compose_clear_then_bubble_and_submit_once() {
  ui::State<std::string> query{"presets"};
  int submissions{};
  ui::UI tree{ui::SearchField{"Recherche", query}.on_submit(
      [&](const std::string &value) {
        NUI_CHECK(value == query.get());
        ++submissions;
      })};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  ui::InputEvent composition;
  composition.type = ui::InputType::Composition;
  composition.composition.type = ui::CompositionType::Start;
  tree.dispatch(composition, platform);
  composition.composition.type = ui::CompositionType::Update;
  composition.composition.text = "marked";
  tree.dispatch(composition, platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(query.get() == "presets");
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(query.get().empty());
  tree.dispatch(test::text("next"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submissions == 1);
  auto up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submissions == 2);
}
void skipped_source_observer_and_invalid_binding_recover_safely() {
  auto query = std::make_unique<ui::State<std::string>>("long original query");
  bool fail = true;
  auto subscription = query->observe([&](const auto &) {
    if (fail)
      throw std::runtime_error("source observer fault");
  });
  int submissions{};
  ui::UI tree{ui::SearchField{"Recherche", *query}.on_submit(
      [&](const auto &) { ++submissions; })};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  bool caught{};
  try {
    query->set("new");
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught);
  fail = false;
  tree.dispatch(test::text("!"), platform);
  NUI_CHECK(query->get() == "new!");
  query.reset();
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(submissions == 0);
  ui::HeadlessRenderer renderer{{480, 80}, 1};
  NUI_CHECK(renderer.render(tree));
}
void submit_source_updates_do_not_rearm_the_same_enter_contact() {
  ui::State<std::string> query{"old"};
  int submits{};
  ui::UI tree{ui::SearchField{"Q", query}.on_submit([&](const auto &) {
    ++submits;
    query.set("external");
  })};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 1);
  auto release = test::key(ui::Key::Enter);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 2);
}
void suite() {
  submit_source_updates_do_not_rearm_the_same_enter_contact();
  escapes_compose_clear_then_bubble_and_submit_once();
  skipped_source_observer_and_invalid_binding_recover_safely();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "submit_latch")
    return test::run(
        "search_field_submit_latch",
        &submit_source_updates_do_not_rearm_the_same_enter_contact);
  return test::run("widget_search_field", &suite);
}
