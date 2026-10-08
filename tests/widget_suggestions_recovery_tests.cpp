#include "test_support.hpp"
#include <nativeui/autocomplete.hpp>
#include <nativeui/editable_combo_box.hpp>
namespace {
void source_write_then_render_closes_the_exact_session() {
  ui::State<std::string> value{""};
  int calls{}, submits{};
  ui::UI tree{ui::Autocomplete{
      "City", value, [&] {
        ++calls;
        return std::vector<std::string>{"Paris", "Pau"};
      }}.on_submit([&](const auto &) { ++submits; })};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  tree.dispatch(test::text("pa"), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1 && calls == 1);
  value.set("external");
  ui::HeadlessRenderer renderer{{480, 350}, 1};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.overlay_entries().empty() && value.get() == "external" &&
            calls == 1 && submits == 0);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("pa"), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1 && calls == 2);
  ui::InputEvent idle;
  idle.type = ui::InputType::Command;
  tree.dispatch(idle, platform);
  NUI_CHECK(tree.overlay_entries().size() == 1 && value.get() == "pa");
}
void provider_failure_keeps_freeform_and_retries_only_explicitly() {
  ui::State<std::string> value{""};
  bool fail{};
  int calls{};
  ui::UI tree{ui::Autocomplete{"City", value, [&] {
                                 ++calls;
                                 if (fail)
                                   throw std::runtime_error("provider fault");
                                 return std::vector<std::string>{"Paris",
                                                                 "Pau"};
                               }}};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  tree.dispatch(test::text("p"), platform);
  fail = true;
  bool caught{};
  try {
    tree.dispatch(test::text("a"), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == "pa" && calls == 2);
  ui::HeadlessRenderer renderer{{480, 350}, 1};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.overlay_entries().empty() && calls == 2 &&
            value.get() == "pa");
  fail = false;
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(calls == 3);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "Paris" && tree.overlay_entries().empty());
}
void composition_commits_one_provider_generation() {
  ui::State<std::string> value{""};
  int calls{};
  ui::UI tree{ui::Autocomplete{"City", value, [&] {
                                 ++calls;
                                 return std::vector<std::string>{"Paris",
                                                                 "Pau"};
                               }}};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  ui::InputEvent event;
  event.type = ui::InputType::Composition;
  event.composition.type = ui::CompositionType::Start;
  tree.dispatch(event, platform);
  event.composition.type = ui::CompositionType::Update;
  event.composition.text = "pa";
  tree.dispatch(event, platform);
  NUI_CHECK(calls == 0 && value.get().empty());
  event.composition.type = ui::CompositionType::Commit;
  tree.dispatch(event, platform);
  NUI_CHECK(calls == 1 && value.get() == "pa");
  tree.dispatch(test::text("pa"), platform);
  NUI_CHECK(calls == 1 && value.get() == "pa");
}
void read_only_and_expired_sources_block_selection() {
  ui::State<std::string> selected{"Unknown"};
  ui::State<bool> read_only{true};
  ui::UI tree{ui::ReadOnly{
      read_only,
      ui::EditableComboBox{"Choice", selected,
                           std::vector<std::string>{"Paris", "Pau"}}}};
  test::MockPlatform platform;
  tree.resize({480, 350});
  tree.activate(platform);
  tree.dispatch(test::text("pa"), platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(selected.get() == "Unknown" && tree.overlay_entries().empty());
  read_only.set(false);
  tree.resize({480, 350});
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("pa"), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  read_only.set(true);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get() == "Unknown" && tree.overlay_entries().empty());
  auto owner = std::make_unique<ui::State<std::string>>("");
  int submits{};
  ui::UI expired{
      ui::Autocomplete{"City", *owner, std::vector<std::string>{"Paris"}}
          .on_submit([&](const auto &) { ++submits; })};
  expired.resize({480, 350});
  expired.activate(platform);
  expired.dispatch(test::text("pa"), platform);
  owner.reset();
  expired.dispatch(test::key(ui::Key::Down), platform);
  expired.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 0 && expired.overlay_entries().empty());
}
void suite() {
  source_write_then_render_closes_the_exact_session();
  provider_failure_keeps_freeform_and_retries_only_explicitly();
  composition_commits_one_provider_generation();
  read_only_and_expired_sources_block_selection();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "source_render")
    return test::run("suggestions_source_render",
                     &source_write_then_render_closes_the_exact_session);
  return test::run("widget_suggestions_recovery", &suite);
}
