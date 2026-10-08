#include "test_support.hpp"
#include <nativeui/number_input.hpp>

#include <limits>

namespace ui {
struct TreeTestAccess {
  static std::string numeric_draft(Tree &tree) {
    return tree.root_->component->semantics().text_value.value_or(std::string{});
  }
  static bool numeric_invalid(Tree &tree) {
    return tree.root_->component->semantics().description.has_value();
  }
  static std::pair<Rect, Rect> input_and_stepper(Tree &tree) {
    return {tree.root_->children[0]->bounds,
            tree.root_->children[1]->bounds};
  }
};
} // namespace ui

namespace {
void replace(ui::UI &tree, test::MockPlatform &platform, std::string text) {
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  if (text.empty()) tree.dispatch(test::key(ui::Key::Backspace), platform);
  else tree.dispatch(test::text(std::move(text)), platform);
}
void stepper_pointer_controls_publish_the_shared_value() {
  ui::State<double> value{12.5};
  ui::UI tree{ui::NumberInput{"Quantity", value}
                  .range(0, 100)
                  .step(0.5)
                  .precision(1)};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 438, 36), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 438, 36), platform);
  NUI_CHECK(value.get() == 13.0);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 438, 58), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 438, 58), platform);
  NUI_CHECK(value.get() == 12.5);
  tree.deactivate(platform);
}
void stepper_aligns_with_the_numeric_field() {
  ui::State<double> value{12.5};
  ui::Tree tree{ui::compile(ui::make_spec(ui::NumberInput{"Quantity", value}))};
  tree.mount();
  tree.layout({450, 82});
  const auto [input, stepper] = ui::TreeTestAccess::input_and_stepper(tree);
  NUI_CHECK(stepper.h <= 48);
  NUI_CHECK_NEAR(stepper.y + stepper.h * 0.5f, input.y + 47.0f,
                 0.01f);
}
void stepper_follows_focused_field_geometry() {
  ui::State<double> value{12.5};
  ui::NumberInputStyle style;
  style.text_input.focused.field_top = 0.0f;
  style.text_input.focused.field_height = 40.0f;
  ui::Tree tree{ui::compile(ui::make_spec(
      ui::NumberInput{"Quantity", value}.style(std::move(style))))};
  test::MockPlatform platform;
  tree.mount();
  tree.layout({450, 82});
  tree.activate_focus(platform);
  tree.layout({450, 82});
  const auto [input, stepper] = ui::TreeTestAccess::input_and_stepper(tree);
  NUI_CHECK_NEAR(stepper.y + stepper.h * 0.5f, input.y + 20.0f,
                 0.01f);
  tree.deactivate_focus(platform);
}
void complete_finite_ascii_drafts_publish_without_step_snap() {
  ui::State<double> value{4.0};
  int submits{};
  ui::UI tree{
      ui::NumberInput{"Copies", value}.range(-100, 100).step(1.0).on_submit(
          [&](double) { ++submits; })};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  for (const std::string text :
       {"", "-", "1e", "1,2", "nan", "inf", "1.0x", "1e9999", "101", "++1", "+-1", "0x1p0", "2e-324"}) {
    replace(tree, platform, text);
    NUI_CHECK(value.get() == 4.0);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(submits == 0 && value.get() == 4.0);
    auto up = test::key(ui::Key::Enter);
    up.type = ui::InputType::KeyUp;
    tree.dispatch(up, platform);
  }
  replace(tree, platform, " +1.25e1 ");
  NUI_CHECK(value.get() == 12.5);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 1 && value.get() == 12.5);
  ui::HeadlessRenderer renderer{{450, 82}, 1};
  NUI_CHECK(renderer.render(tree));
}
void external_source_wins_escape_and_live_exception_recovers() {
  ui::State<double> value{2.0};
  bool fail{};
  auto subscription = value.observe([&](double) {
    if (fail)
      throw std::runtime_error("injected NumberInput value observer");
  });
  ui::UI tree{ui::NumberInput{"Count", value}.range(0, 10).precision(0)};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  replace(tree, platform, "-");
  value.set(7.0);
  replace(tree, platform, "8");
  NUI_CHECK(value.get() == 8.0);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == 7.0);
  fail = true;
  bool caught{};
  try {
    replace(tree, platform, "5");
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == 5.0);
  fail = false;
  replace(tree, platform, "6");
  NUI_CHECK(value.get() == 6.0);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == 7.0);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(value.get() == 6.0);
}
void composition_preedit_is_not_parsed_and_invalid_options_reject() {
  ui::State<double> value{1};
  ui::UI tree{ui::NumberInput{"Count", value}.range(0, 100)};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  ui::InputEvent event;
  event.type = ui::InputType::Composition;
  event.composition.type = ui::CompositionType::Start;
  tree.dispatch(event, platform);
  event.composition.type = ui::CompositionType::Update;
  event.composition.text = "12";
  tree.dispatch(event, platform);
  NUI_CHECK(value.get() == 1.0);
  event.composition.type = ui::CompositionType::Commit;
  tree.dispatch(event, platform);
  NUI_CHECK(value.get() == 12.0);
  bool rejected{};
  try {
    auto bad = ui::NumberInput{"N", value}.precision(18);
    (void)bad;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
  rejected = false;
  try {
    auto bad = ui::NumberInput{"N", value}.step(
        std::numeric_limits<double>::infinity());
    (void)bad;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void extreme_ranges_and_read_only_do_not_publish_nonfinite_values() {
  ui::State<double> value{std::numeric_limits<double>::max()};
  ui::UI tree{ui::NumberInput{"Extreme", value}
                  .range(-std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max())
                  .step(std::numeric_limits<double>::denorm_min())
                  .precision(17)};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(std::isfinite(value.get()));
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(std::isfinite(value.get()));
  ui::State<double> constant{4};
  ui::UI fixed{ui::NumberInput{"Fixed", constant}.range(4, 4)};
  fixed.resize({450, 82});
  fixed.activate(platform);
  replace(fixed, platform, "9");
  NUI_CHECK(constant.get() == 4);
  value.set(std::numeric_limits<double>::quiet_NaN());
  tree.resize({450, 82});
  ui::HeadlessRenderer renderer{{450, 82}, 1};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(std::isnan(value.get()));
}
void passive_rounding_preserves_numeric_submit_and_escape_baseline() {
  ui::State<double> value{1.25};
  std::vector<double> submitted;
  ui::UI tree{ui::NumberInput{"N", value}.range(0, 10).precision(0).on_submit(
      [&](double v) { submitted.push_back(v); })};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submitted.size() == 1 && submitted[0] == 1.25 &&
            value.get() == 1.25);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == 1.25);
}
void an_explicit_edit_matching_rounded_display_still_publishes() {
  ui::State<double> value{1.25};
  ui::UI tree{ui::NumberInput{"N", value}.range(0, 10).precision(0)};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  replace(tree, platform, "1");
  NUI_CHECK(value.get() == 1.0);
}
void submit_source_updates_do_not_rearm_the_same_enter_contact() {
  ui::State<double> value{1};
  int submits{};
  ui::UI tree{ui::NumberInput{"N", value}.on_submit([&](double) {
    ++submits;
    value.set(value.get() + 1);
  })};
  test::MockPlatform platform;
  tree.resize({450, 82});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 1 && value.get() == 2);
  auto release = test::key(ui::Key::Enter);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 2 && value.get() == 3);
}
void throwing_invalidation_does_not_publish_speculative_step() {
  ui::State<double> value{12.5};
  ui::Tree tree{ui::compile(ui::make_spec(
      ui::NumberInput{"Quantity", value}.range(0, 100).step(0.5).precision(1)))};
  test::MockPlatform platform;
  tree.mount();
  tree.layout({450, 82});
  tree.activate_focus(platform);

  bool fail = false;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (fail)
      throw std::runtime_error("injected numeric step invalidation failure");
  });
  NUI_CHECK(ui::TreeTestAccess::numeric_draft(tree) == "12.5");

  fail = true;
  bool caught = false;
  try {
    (void)tree.dispatch(test::key(ui::Key::Up), platform);
  } catch (const std::runtime_error&) {
    caught = true;
  }
  NUI_CHECK(caught);
  NUI_CHECK(value.get() == 12.5);
  NUI_CHECK(ui::TreeTestAccess::numeric_draft(tree) == "12.5");

  fail = false;
  (void)tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == 13.0);
  NUI_CHECK(ui::TreeTestAccess::numeric_draft(tree) == "13.0");
  tree.deactivate_focus(platform);
}

void edge_step_normalizes_invalid_draft_without_changing_source() {
  ui::State<double> value{100.0};
  ui::Tree tree{ui::compile(ui::make_spec(
      ui::NumberInput{"Quantity", value}.range(0, 100).step(1).precision(0)))};
  test::MockPlatform platform;
  tree.mount();
  tree.layout({450, 82});
  tree.activate_focus(platform);

  ui::InputEvent select_all;
  select_all.type = ui::InputType::Command;
  select_all.command = ui::Command::SelectAll;
  (void)tree.dispatch(select_all, platform);
  (void)tree.dispatch(test::text("invalid"), platform);
  NUI_CHECK(ui::TreeTestAccess::numeric_invalid(tree));
  NUI_CHECK(value.get() == 100.0);

  (void)tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == 100.0);
  NUI_CHECK(ui::TreeTestAccess::numeric_draft(tree) == "100");
  NUI_CHECK(!ui::TreeTestAccess::numeric_invalid(tree));
  tree.deactivate_focus(platform);
}

void suite() {
  throwing_invalidation_does_not_publish_speculative_step();
  edge_step_normalizes_invalid_draft_without_changing_source();
  stepper_aligns_with_the_numeric_field();
  stepper_follows_focused_field_geometry();
  stepper_pointer_controls_publish_the_shared_value();
  submit_source_updates_do_not_rearm_the_same_enter_contact();
  passive_rounding_preserves_numeric_submit_and_escape_baseline();
  an_explicit_edit_matching_rounded_display_still_publishes();
  extreme_ranges_and_read_only_do_not_publish_nonfinite_values();
  complete_finite_ascii_drafts_publish_without_step_snap();
  external_source_wins_escape_and_live_exception_recovers();
  composition_preedit_is_not_parsed_and_invalid_options_reject();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "submit_latch")
    return test::run(
        "number_input_submit_latch",
        &submit_source_updates_do_not_rearm_the_same_enter_contact);
  if (argc == 2 && std::string_view{argv[1]} == "precision")
    return test::run(
        "number_input_precision",
        &passive_rounding_preserves_numeric_submit_and_escape_baseline);
  if (argc == 2 && std::string_view{argv[1]} == "equivalent_edit")
    return test::run(
        "number_input_equivalent_edit",
        &an_explicit_edit_matching_rounded_display_still_publishes);
  return test::run("widget_number_input", &suite);
}
