#include "group_test_support.hpp"
#include <nativeui/text_input.hpp>
#include <nativeui/toggle_button.hpp>
#include <nativeui/toggle_group.hpp>
namespace {
ui::Spec controls(ui::State<bool> &first, ui::State<bool> &second,
                  const std::shared_ptr<group_test::Audit> &audit,
                  ui::State<bool> *enabled = nullptr) {
  std::vector<ui::Spec> items;
  items.push_back(ui::ToggleButton{"Bold", first}.spec());
  auto middle =
      group_test::probe(audit, ui::ToggleButton{"Italic", second}.spec());
  if (enabled)
    middle = ui::Enabled{*enabled, std::move(middle)}.spec();
  items.push_back(std::move(middle));
  items.push_back(ui::Button{"Underline", [] {}}.spec());
  return ui::ToggleGroup{"Formatting", std::move(items)}.spec();
}
void roving_is_one_stop_and_never_changes_values() {
  ui::State<bool> bold{false}, italic{false};
  auto audit = std::make_shared<group_test::Audit>();
  ui::UI tree{ui::Column{ui::Button{"Before", {}},
                         controls(bold, italic, audit),
                         ui::Button{"After", {}}}};
  test::MockPlatform platform;
  tree.resize({640, 180});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "Bold");
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Italic");
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(group_test::focused(tree) == "Underline");
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Bold");
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(group_test::focused(tree) == "Underline");
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(group_test::focused(tree) == "Bold");
  NUI_CHECK(!bold.get() && !italic.get());
  group_test::click(tree, platform, audit->bounds, false);
  NUI_CHECK(group_test::focused(tree) == "Italic");
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "After");
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  NUI_CHECK(group_test::focused(tree) == "Italic");
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(!italic.get());
  group_test::release(tree, platform, ui::Key::Space);
  NUI_CHECK(italic.get() && !bold.get());
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void disabled_and_readonly_are_focus_only() {
  ui::State<bool> bold{false}, italic{false}, enabled{false}, readonly{true};
  auto audit = std::make_shared<group_test::Audit>();
  ui::UI tree{ui::Column{
      ui::Button{"Before", {}},
      ui::ReadOnly{readonly, controls(bold, italic, audit, &enabled)},
      ui::Button{"After", {}}}};
  test::MockPlatform platform;
  tree.resize({640, 180});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Underline" && !bold.get() &&
            !italic.get());
  enabled.set(true);
  group_test::render(tree);
  tree.dispatch(test::key(ui::Key::Left), platform);
  NUI_CHECK(group_test::focused(tree) == "Italic");
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!italic.get());
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  NUI_CHECK(group_test::focused(tree) == "Italic");
}
void editor_retains_its_own_navigation() {
  ui::State<std::string> text{"abc"};
  ui::State<bool> bold{false};
  auto editor = std::make_shared<group_test::Audit>();
  std::vector<ui::Spec> items;
  items.push_back(ui::ToggleButton{"Bold", bold}.spec());
  items.push_back(
      group_test::probe(editor, ui::TextInput{"Editor", text}.spec()));
  items.push_back(ui::Button{"Last", {}}.spec());
  ui::UI tree{ui::ToggleGroup{"Formatting", std::move(items)}};
  test::MockPlatform platform;
  tree.resize({640, 80});
  tree.activate(platform);
  group_test::click(tree, platform, editor->bounds, false);
  tree.dispatch(test::key(ui::Key::Home), platform);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::text("x"), platform);
  NUI_CHECK(text.get() == "axbc" && !bold.get());
  NUI_CHECK(group_test::focused(tree) == "Editor");
}
void removal_of_armed_segment_cancels_and_recovers() {
  ui::State<bool> shown{true}, bold{false}, italic{false};
  auto audit = std::make_shared<group_test::Audit>();
  std::vector<ui::Spec> items;
  items.push_back(ui::ToggleButton{"Bold", bold}.spec());
  items.push_back(ui::If{
      shown,
      group_test::probe(audit, ui::ToggleButton{"Italic", italic}.spec())}
                      .spec());
  items.push_back(ui::Button{"Last", {}}.spec());
  ui::UI tree{ui::ToggleGroup{"Formatting", std::move(items)}};
  test::MockPlatform platform;
  tree.resize({640, 80});
  tree.activate(platform);
  group_test::render(tree, {640, 80});
  auto b = audit->bounds;
  tree.dispatch(test::pointer(ui::InputType::PointerDown, b.x + b.w * .5f,
                              b.y + b.h * .5f),
                platform);
  shown.set(false);
  group_test::render(tree, {640, 80});
  tree.dispatch(
      test::pointer(ui::InputType::PointerUp, b.x + b.w * .5f, b.y + b.h * .5f),
      platform);
  NUI_CHECK(!italic.get() && group_test::focused(tree) == "Last");
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  shown.set(true);
  group_test::render(tree, {640, 80});
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(group_test::focused(tree) == "Bold");
}
void copied_recipe_has_independent_focus_memory() {
  ui::State<bool> bold{false}, italic{false};
  auto audit = std::make_shared<group_test::Audit>();
  auto spec = ui::Column{ui::Button{"Before", {}},
                         controls(bold, italic, audit), ui::Button{"After", {}}}
                  .spec();
  ui::UI left{ui::Spec{spec}}, right{ui::Spec{spec}};
  test::MockPlatform a, b;
  left.resize({640, 180});
  right.resize({640, 180});
  left.activate(a);
  right.activate(b);
  left.dispatch(test::key(ui::Key::Tab), a);
  left.dispatch(test::key(ui::Key::Right), a);
  right.dispatch(test::key(ui::Key::Tab), b);
  right.dispatch(test::key(ui::Key::End), b);
  left.dispatch(test::key(ui::Key::Tab), a);
  left.dispatch(test::key(ui::Key::Tab, true), a);
  NUI_CHECK(group_test::focused(left) == "Italic" &&
            group_test::focused(right) == "Underline");
  NUI_CHECK(!bold.get() && !italic.get());
}
void empty_and_throwing_action_recover() {
  ui::UI empty{ui::ToggleGroup{"Empty", {}}};
  test::MockPlatform platform;
  empty.resize({1, 40});
  empty.activate(platform);
  group_test::render(empty, {1, 40});
  NUI_CHECK(group_test::focused(empty).empty());
  int calls{};
  bool fail{true};
  std::vector<ui::Spec> items;
  items.push_back(ui::Button{
      "Throw", [&] {
        ++calls;
        if (fail)
          throw std::runtime_error("group child action");
      }}.spec());
  items.push_back(ui::Button{"Next", [&] { ++calls; }}.spec());
  ui::UI tree{ui::ToggleGroup{"Actions", std::move(items)}};
  tree.resize({320, 80});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1);
  group_test::release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 2);
  group_test::render(tree, {320, 80});
}
void real_group_scopes_are_local_and_nearest() {
  ui::ToggleGroupStyle outer, inner;
  outer.padding = inner.padding = 0;
  outer.segment.base.minimum_width = 203;
  inner.segment.base.minimum_width = 117;
  outer.segment.base.text_size = inner.segment.base.text_size = 1;
  outer.segment.base.horizontal_padding =
      inner.segment.base.horizontal_padding = 0;
  std::vector<ui::Spec> nested_children;
  nested_children.push_back(ui::Button{"x", {}}.spec());
  auto nested =
      ui::ToggleGroup{"Same", std::move(nested_children)}.style(inner).spec();
  std::vector<ui::Spec> children;
  children.push_back(std::move(nested));
  ui::UI tree{ui::ToggleGroup{"Same", std::move(children)}.style(outer)};
  NUI_CHECK_NEAR(tree.measure().preferred.w, 117, .01f);
  ui::UI ordinary{ui::Button{"x", {}}};
  NUI_CHECK(ordinary.measure().preferred.w < 203);
  ui::ButtonStyle own;
  own.base.minimum_width = 71;
  std::vector<ui::Spec> explicit_children;
  explicit_children.push_back(ui::Button{"x", {}}.style(own).spec());
  ui::UI explicit_style{
      ui::ToggleGroup{"Same", std::move(explicit_children)}.style(outer)};
  NUI_CHECK_NEAR(explicit_style.measure().preferred.w, 71, .01f);
}
void partial_child_mount_failure_unwinds_then_recipe_recovers() {
  auto first = std::make_shared<group_test::Audit>();
  auto second = std::make_shared<group_test::Audit>();
  auto third = std::make_shared<group_test::Audit>();
  std::vector<ui::Spec> children;
  children.push_back(group_test::probe(first, ui::Button{"First", {}}.spec()));
  children.push_back(
      group_test::probe(second, ui::Button{"Second", {}}.spec()));
  children.push_back(group_test::probe(third, ui::Button{"Third", {}}.spec()));
  auto recipe = ui::ToggleGroup{"Recovery", std::move(children)}.spec();
  second->throw_mount = true;
  bool caught{};
  try {
    ui::UI failed{ui::Spec{recipe}};
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && first->mounts == 1 && first->unmounts == 1);
  NUI_CHECK(second->mounts == 1 && second->unmounts == 1);
  NUI_CHECK(third->mounts == 0 && third->unmounts == 0);
  second->throw_mount = false;
  {
    ui::UI recovered{ui::Spec{recipe}};
    test::MockPlatform platform;
    recovered.resize({400, 70});
    recovered.activate(platform);
    recovered.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(group_test::focused(recovered) == "Second");
    group_test::render(recovered, {400, 70});
  }
  NUI_CHECK(first->mounts == 2 && first->unmounts == 2);
  NUI_CHECK(second->mounts == 2 && second->unmounts == 2);
  NUI_CHECK(third->mounts == 1 && third->unmounts == 1);
}
void suite() {
  roving_is_one_stop_and_never_changes_values();
  disabled_and_readonly_are_focus_only();
  editor_retains_its_own_navigation();
  removal_of_armed_segment_cancels_and_recovers();
  copied_recipe_has_independent_focus_memory();
  empty_and_throwing_action_recover();
  real_group_scopes_are_local_and_nearest();
  partial_child_mount_failure_unwinds_then_recipe_recovers();
}
} // namespace
int main() { return test::run("widget_toggle_group", &suite); }
