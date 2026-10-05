#include "test_support.hpp"
#include <nativeui/color_picker.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/read_only.hpp>
namespace {
ui::InputEvent composition(ui::CompositionType type, std::string text = {}) {
  ui::InputEvent event;
  event.type = ui::InputType::Composition;
  event.composition.type = type;
  event.composition.text = std::move(text);
  return event;
}
void hexadecimal_composition_owns_submit_and_cancel_keys() {
  const ui::Color original{0, 0, 1, 1};
  ui::State<ui::Color> color{original};
  int calls{};
  ui::UI tree{ui::ColorPicker{"Palette", color}.on_change(
      [&](ui::Color) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({280, 420});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 360), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 360), platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("#00ff00"), platform);
  tree.dispatch(composition(ui::CompositionType::Start), platform);
  tree.dispatch(composition(ui::CompositionType::Update, "80"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(color.get() == original && calls == 0);
  tree.dispatch(composition(ui::CompositionType::Commit, "80"), platform);
  tree.dispatch(test::text("80"), platform);
  NUI_CHECK(color.get() == original && calls == 0);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  const ui::Color green{0, 1, 0, 128.f / 255};
  NUI_CHECK(color.get() == green && calls == 1);

  tree.dispatch(all, platform);
  tree.dispatch(test::text("#ff0000"), platform);
  tree.dispatch(composition(ui::CompositionType::Start), platform);
  tree.dispatch(composition(ui::CompositionType::Update, "ff"), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(color.get() == green && calls == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(color.get() == ui::Color({1, 0, 0, green.a}) && calls == 2);
}
void compact_hex_does_not_overlap_swatches() {
  ui::State<ui::Color> color{ui::Color{0, 0, 1, 1}};
  ui::UI tree{ui::ColorPicker{"Palette", color}.swatches(
      {{"red", "Red", {1, 0, 0, 1}}})};
  test::MockPlatform platform;
  tree.resize({280, 420});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{280, 420}, 1};
  NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 360), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 360), platform);
  NUI_CHECK(platform.text_input_active);
  NUI_CHECK_NEAR(platform.text_input_area.y, 344, .01f);
  NUI_CHECK_NEAR(platform.text_input_area.h, 32, .01f);
  ui::InputEvent all{};
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("#00ff00ff"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(color.get() == ui::Color({0, 1, 0, 1}));
}
void swatches_show_their_color_and_select_it() {
  ui::State<ui::Color> color{ui::Color{0, 0, 1, 1}};
  ui::UI tree{ui::ColorPicker{"Palette", color}.swatches(
      {{"red", "Red", {1, 0, 0, 1}}})};
  test::MockPlatform platform;
  tree.resize({280, 420});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{280, 420}, 1};
  NUI_CHECK(renderer.render(tree));
  std::optional<ui::SemanticInfo> swatch;
  for (ui::NodeId id = 1; id < 20; ++id)
    if (auto info = tree.component_semantics(id); info && info->name == "Red")
      swatch = std::move(info);
  NUI_CHECK(swatch && swatch->enabled);
  const auto pixel = renderer.pixel(22, 398);
  NUI_CHECK(pixel.r > 240 && pixel.g < 15 && pixel.b < 15);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 22, 398), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 22, 398), platform);
  NUI_CHECK(color.get() == ui::Color({1, 0, 0, 1}));
}
void swatches_respect_owner_availability_and_expiration() {
  auto color = std::make_unique<ui::State<ui::Color>>(ui::Color{0, 0, 1, 1});
  ui::State<bool> enabled{false}, read_only{false};
  int calls{};
  ui::UI tree{ui::Enabled{
      enabled,
      ui::ReadOnly{read_only, ui::ColorPicker{"Palette", color->binding()}
                                  .swatches({{"red", "Red", {1, 0, 0, 1}}})
                                  .on_change([&](ui::Color) { ++calls; })}}};
  test::MockPlatform platform;
  tree.resize({280, 420});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{280, 420}, 1};
  auto click = [&] {
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 22, 398), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 22, 398), platform);
  };
  NUI_CHECK(renderer.render(tree));
  click();
  NUI_CHECK(calls == 0 && color->get() == ui::Color({0, 0, 1, 1}));
  enabled.set(true);
  read_only.set(true);
  NUI_CHECK(renderer.render(tree));
  click();
  NUI_CHECK(calls == 0 && color->get() == ui::Color({0, 0, 1, 1}));
  read_only.set(false);
  NUI_CHECK(renderer.render(tree));
  click();
  NUI_CHECK(calls == 1 && color->get() == ui::Color({1, 0, 0, 1}));
  color.reset();
  NUI_CHECK(renderer.render(tree));
  click();
  NUI_CHECK(calls == 1);
  bool found = false;
  for (ui::NodeId id = 1; id < 20; ++id)
    if (const auto info = tree.component_semantics(id);
        info && info->name == "Red") {
      found = true;
      NUI_CHECK(!info->enabled);
    }
  NUI_CHECK(found);
}
void suite() {
  hexadecimal_composition_owns_submit_and_cancel_keys();
  swatches_respect_owner_availability_and_expiration();
  swatches_show_their_color_and_select_it();
  compact_hex_does_not_overlap_swatches();
  const ui::Color original{.1234f, .3456f, .5678f, .4321f};
  ui::State<ui::Color> color{original};
  int calls{};
  ui::UI tree{ui::ColorPicker{"Color", color}.alpha_enabled(false).on_change(
      [&](ui::Color c) {
        NUI_CHECK(c == color.get());
        ++calls;
      })};
  test::MockPlatform platform;
  tree.resize({280, 420});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{280, 420}, 1};
  NUI_CHECK(renderer.render(tree) && color.get() == original && calls == 0);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(color.get().a == original.a && calls == 1);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  ui::InputEvent all{};
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("#ff000080"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1 && color.get().a == original.a);
  tree.dispatch(all, platform);
  tree.dispatch(test::text("#00ff00"), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(color.get() == ui::Color({0, 1, 0, original.a}) && calls == 2);
  bool rejected{};
  try {
    (void)ui::ColorPicker{"Bad", color}
        .swatches({{"x", "A", original}, {"x", "B", original}})
        .spec();
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
} // namespace
int main() { return test::run("widget_color_picker", &suite); }
