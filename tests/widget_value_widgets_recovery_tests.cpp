#include "test_support.hpp"
#include <nativeui/calendar.hpp>
#include <nativeui/color_picker.hpp>
#include <nativeui/time_input.hpp>
#include <nativeui/token_field.hpp>
namespace {
void time_observer_failure_is_not_replayed() {
  ui::State<ui::TimeInput::Value> value{std::chrono::seconds{0}};
  bool fail = true;
  int calls{};
  auto first = value.observe([&](const auto &) {
    if (fail)
      throw std::runtime_error("observer");
  });
  ui::UI tree{ui::TimeInput{"Time", value}.on_change([&](auto) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({300, 80});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Up), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == std::chrono::seconds{3600} && calls == 0);
  ui::HeadlessRenderer renderer{{300, 80}, 1};
  NUI_CHECK(renderer.render(tree) && calls == 0);
  fail = false;
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{7200} && calls == 1);
}
void token_batch_failure_keeps_the_committed_vector() {
  ui::State<std::vector<std::string>> value{std::vector<std::string>{}};
  bool fail = true;
  int publications{};
  auto first = value.observe([&](const auto &) {
    ++publications;
    if (fail)
      throw std::runtime_error("observer");
  });
  ui::UI tree{ui::TokenField{"Tags", value}};
  test::MockPlatform platform;
  tree.resize({300, 160});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::text("a,b,tail"), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == std::vector<std::string>({"a", "b"}) &&
            publications == 1);
  ui::HeadlessRenderer renderer{{300, 160}, 1};
  NUI_CHECK(renderer.render(tree) && publications == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == std::vector<std::string>({"a", "b", "tail"}) &&
            publications == 2);
}
void color_external_update_cancels_the_old_contact() {
  ui::State<ui::Color> value{ui::Color{1, 0, 0, .3f}};
  int calls{};
  ui::UI tree{
      ui::ColorPicker{"Color", value}.on_change([&](auto) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({280, 430});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 120, 120), platform);
  NUI_CHECK(calls == 1 && platform.pointer_capture_begin_count == 1);
  const ui::Color external{0, 1, 0, .3f};
  value.set(external);
  NUI_CHECK(platform.pointer_capture_end_count == 1);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 180, 180), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 180, 180), platform);
  NUI_CHECK(value.get() == external && calls == 1);
}
void gray_colors_keep_the_instance_hue() {
  ui::State<ui::Color> value{ui::Color{0, 0, 1, 1}};
  ui::UI tree{ui::ColorPicker{"Color", value}};
  test::MockPlatform platform;
  tree.resize({280, 430});
  tree.activate(platform);
  value.set({.5f, .5f, .5f, 1});
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get().b > value.get().r && value.get().b > value.get().g);
}
void composition_interprets_commas_only_at_commit() {
  ui::State<std::vector<std::string>> value{std::vector<std::string>{}};
  int publications{};
  auto observer = value.observe([&](const auto &) { ++publications; });
  ui::UI tree{ui::TokenField{"Tags", value}};
  test::MockPlatform platform;
  tree.resize({300, 160});
  tree.activate(platform);
  ui::InputEvent event;
  event.type = ui::InputType::Composition;
  event.composition.type = ui::CompositionType::Start;
  tree.dispatch(event, platform);
  event.composition.type = ui::CompositionType::Update;
  event.composition.text = "a,b,";
  tree.dispatch(event, platform);
  NUI_CHECK(value.get().empty() && publications == 0);
  event.composition.type = ui::CompositionType::Commit;
  tree.dispatch(event, platform);
  NUI_CHECK(value.get() == std::vector<std::string>({"a", "b"}) &&
            publications == 1);
}
void suite() {
  time_observer_failure_is_not_replayed();
  token_batch_failure_keeps_the_committed_vector();
  color_external_update_cancels_the_old_contact();
  gray_colors_keep_the_instance_hue();
  composition_interprets_commas_only_at_commit();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "color_external")
    return test::run("color_external",
                     &color_external_update_cancels_the_old_contact);
  if (argc == 2 && std::string_view{argv[1]} == "token_failure")
    return test::run("token_failure",
                     &token_batch_failure_keeps_the_committed_vector);
  return test::run("widget_value_widgets_recovery", &suite);
}
