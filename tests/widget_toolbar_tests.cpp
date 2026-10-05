#include "group_test_support.hpp"
#include <nativeui/flex.hpp>
#include <nativeui/spacer.hpp>
#include <nativeui/text_input.hpp>
#include <nativeui/toggle_button.hpp>
#include <nativeui/toggle_group.hpp>
#include <nativeui/toolbar.hpp>
namespace {
ui::ButtonStyle button_style() {
  ui::ButtonStyle s;
  s.base.minimum_width = 60.0f;
  s.base.control_height = 32.0f;
  s.base.horizontal_padding = 0.0f;
  s.base.text_size = 1.0f;
  return s;
}
ui::ToolbarStyle bar_style() {
  ui::ToolbarStyle s;
  s.padding = 0;
  s.gap = 0;
  s.minimum_height = 32;
  s.overflow_button.base.minimum_width = 48.0f;
  s.overflow_button.base.horizontal_padding = 0.0f;
  s.overflow_button.base.text_size = 1.0f;
  return s;
}
std::vector<ui::ToolbarItem>
items(std::vector<int> &calls,
      const std::vector<std::shared_ptr<group_test::Audit>> &audit,
      bool fail = false) {
  std::vector<ui::ToolbarItem> result;
  const std::vector<std::string> labels{"Save", "Undo", "Redo"};
  for (std::size_t i = 0; i < labels.size(); ++i) {
    auto action = [&, i, fail] {
      ++calls[i];
      if (fail && i == 1)
        throw std::runtime_error("toolbar menu command");
    };
    result.push_back(
        {labels[i],
         group_test::probe(
             audit[i],
             ui::Button{labels[i], action}.style(button_style()).spec()),
         ui::PopupMenuItem::action(labels[i], action)});
  }
  return result;
}
std::vector<std::shared_ptr<group_test::Audit>> audits() {
  return {std::make_shared<group_test::Audit>(),
          std::make_shared<group_test::Audit>(),
          std::make_shared<group_test::Audit>()};
}
bool visible(const ui::UI &tree, std::string_view name) {
  return group_test::node(tree, name, true).has_value();
}
std::optional<ui::SemanticInfo> menu_info(const ui::UI &tree,
                                          std::string_view name) {
  for (ui::NodeId id = 1; id < 8192; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->role == ui::SemanticRole::MenuItem && info->name == name)
      return info;
  }
  return {};
}
void open_more(ui::UI &tree, test::MockPlatform &platform) {
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(group_test::focused(tree) == "More");
  tree.dispatch(test::key(ui::Key::Down), platform);
  group_test::release(tree, platform, ui::Key::Down);
  NUI_CHECK(tree.overlay_entries().size() == 1);
}
void overflow_preserves_suffix_order_and_same_application_commands() {
  std::vector<int> calls(3);
  auto audit = audits();
  ui::UI tree{ui::Toolbar{"Document", items(calls, audit)}.style(bar_style())};
  test::MockPlatform platform;
  tree.resize({300, 60});
  tree.activate(platform);
  group_test::render(tree, {300, 60});
  NUI_CHECK(visible(tree, "Save") && visible(tree, "Undo") &&
            visible(tree, "Redo") && !visible(tree, "More"));
  group_test::click(tree, platform, audit[0]->bounds);
  NUI_CHECK(calls[0] == 1);
  tree.resize({120, 60});
  group_test::render(tree, {120, 60});
  NUI_CHECK(visible(tree, "Save") && !visible(tree, "Undo") &&
            !visible(tree, "Redo") && visible(tree, "More"));
  open_more(tree, platform);
  tree.dispatch(test::key(ui::Key::Home), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[1] == 1 && calls[2] == 0 && tree.overlay_entries().empty());
  group_test::release(tree, platform, ui::Key::Enter);
  open_more(tree, platform);
  tree.dispatch(test::key(ui::Key::End), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[2] == 1 && tree.overlay_entries().empty());
  tree.resize({300, 60});
  group_test::render(tree, {300, 60});
  for (const auto &current : audit)
    NUI_CHECK(current->mounts == 1 && current->unmounts == 0);
}
void one_tab_stop_last_target_and_readonly_command_behavior() {
  std::vector<int> calls(3);
  auto audit = audits();
  ui::State<bool> readonly{true};
  ui::UI tree{ui::Column{
      ui::Button{"Before", {}},
      ui::ReadOnly{readonly, ui::Toolbar{"Document", items(calls, audit)}.style(
                                 bar_style())},
      ui::Button{"After", {}}}};
  test::MockPlatform platform;
  tree.resize({300, 180});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "Save");
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Undo");
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "After");
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  NUI_CHECK(group_test::focused(tree) == "Undo");
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[1] == 1);
}
void focus_and_capture_transfer_to_more_without_remount() {
  ui::State<bool> value{false};
  auto audit = std::make_shared<group_test::Audit>();
  std::vector<ui::ToolbarItem> entries;
  entries.push_back({"save",
                     ui::Button{"Save", {}}.style(button_style()).spec(),
                     ui::PopupMenuItem::action("Save", [] {})});
  ui::ToggleButtonStyle style;
  static_cast<ui::ButtonStyle &>(style) = button_style();
  entries.push_back(
      {"toggle",
       group_test::probe(audit,
                         ui::ToggleButton{"Value", value}.style(style).spec()),
       ui::PopupMenuItem::action("Value", [] {})});
  entries.push_back({"last",
                     ui::Button{"Last", {}}.style(button_style()).spec(),
                     ui::PopupMenuItem::action("Last", [] {})});
  ui::UI tree{ui::Toolbar{"Document", std::move(entries)}.style(bar_style())};
  test::MockPlatform platform;
  tree.resize({300, 60});
  tree.activate(platform);
  const auto old = audit->bounds;
  tree.dispatch(test::pointer(ui::InputType::PointerDown, old.x + old.w * .5f,
                              old.y + old.h * .5f),
                platform);
  NUI_CHECK(group_test::focused(tree) == "Value");
  tree.resize({120, 60});
  group_test::render(tree, {120, 60});
  NUI_CHECK(group_test::focused(tree) == "More" && !value.get());
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, old.x + old.w * .5f,
                              old.y + old.h * .5f),
                platform);
  NUI_CHECK(!value.get());
  open_more(tree, platform);
  tree.resize({150, 60});
  group_test::render(tree, {150, 60});
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.resize({300, 60});
  group_test::render(tree, {300, 60});
  NUI_CHECK(tree.overlay_entries().empty());
  NUI_CHECK(audit->mounts == 1 && audit->unmounts == 0 &&
            visible(tree, "Value"));
}
void nonoverflowable_and_whole_group_metadata_are_preserved() {
  ui::State<bool> bold{true}, italic{false};
  std::vector<ui::Spec> controls;
  ui::ToggleButtonStyle s;
  static_cast<ui::ButtonStyle &>(s) = button_style();
  controls.push_back(ui::ToggleButton{"Bold", bold}.style(s).spec());
  controls.push_back(ui::ToggleButton{"Italic", italic}.style(s).spec());
  ui::PopupMenuItem branch;
  branch.label = "Formatting";
  branch.key = "formatting";
  auto b = ui::PopupMenuItem::action("Bold", [] {});
  b.checked = true;
  auto i = ui::PopupMenuItem::action("Italic", [] {}, false);
  i.checked = false;
  branch.children = {b, i};
  std::vector<ui::ToolbarItem> entries;
  entries.push_back({"mandatory",
                     ui::Button{"Mandatory", {}}.style(button_style()).spec(),
                     {}});
  entries.push_back({"formatting",
                     ui::ToggleGroup{"Formatting", std::move(controls)}.spec(),
                     branch});
  ui::UI tree{ui::Toolbar{"Document", std::move(entries)}.style(bar_style())};
  test::MockPlatform platform;
  tree.resize({120, 70});
  tree.activate(platform);
  group_test::render(tree, {120, 70});
  NUI_CHECK(visible(tree, "Mandatory") && !visible(tree, "Bold") &&
            !visible(tree, "Italic"));
  open_more(tree, platform);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(tree.overlay_entries().size() == 2);
  const auto checked = menu_info(tree, "Bold"),
             disabled = menu_info(tree, "Italic");
  NUI_CHECK(checked && disabled &&
            checked->checked == ui::SemanticCheckedState::Checked &&
            !disabled->enabled);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.resize({20, 70});
  group_test::render(tree, {20, 70});
  NUI_CHECK(visible(tree, "Mandatory"));
}
void layout_failure_keeps_last_committed_plan_then_recovers() {
  std::vector<int> calls(3);
  auto audit = audits();
  ui::UI tree{ui::Toolbar{"Document", items(calls, audit)}.style(bar_style())};
  test::MockPlatform platform;
  tree.resize({300, 60});
  tree.activate(platform);
  const auto old = audit[1]->bounds;
  audit[2]->throw_measure = true;
  bool caught{};
  try {
    tree.resize({120, 60});
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && visible(tree, "Undo") && !visible(tree, "More") &&
            group_test::same_rect(old, audit[1]->bounds));
  audit[2]->throw_measure = false;
  tree.resize({120, 60});
  group_test::render(tree, {120, 60});
  NUI_CHECK(!visible(tree, "Undo") && visible(tree, "More"));
  open_more(tree, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[1] == 1);
}
void old_menu_and_throwing_command_are_terminal() {
  std::vector<int> calls(3);
  auto audit = audits();
  ui::State<bool> shown{true};
  ui::UI removed{ui::If{
      shown, ui::Toolbar{"Document", items(calls, audit)}.style(bar_style())}};
  test::MockPlatform platform;
  removed.resize({120, 60});
  removed.activate(platform);
  group_test::render(removed, {120, 60});
  open_more(removed, platform);
  shown.set(false);
  group_test::render(removed, {120, 60});
  removed.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[1] == 0 && removed.overlay_entries().empty());
  ui::UI tree{
      ui::Toolbar{"Document", items(calls, audit, true)}.style(bar_style())};
  tree.resize({120, 60});
  tree.activate(platform);
  open_more(tree, platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls[1] == 1 && tree.overlay_entries().empty());
  group_test::render(tree, {120, 60});
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[1] == 1);
  group_test::release(tree, platform, ui::Key::Enter);
  open_more(tree, platform);
  tree.dispatch(test::key(ui::Key::End), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls[2] == 1);
}
void duplicate_keys_are_rejected() {
  std::vector<ui::ToolbarItem> entries;
  entries.push_back({"same", ui::Button{"One", {}}.spec(), {}});
  entries.push_back({"same", ui::Button{"Two", {}}.spec(), {}});
  bool rejected{};
  try {
    auto bad = ui::Toolbar{"Bad", std::move(entries)}.spec();
    (void)bad;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void copied_recipe_has_independent_plan_focus_and_menu() {
  std::vector<int> calls(3);
  auto audit = audits();
  auto spec =
      ui::Toolbar{"Document", items(calls, audit)}.style(bar_style()).spec();
  ui::UI left{ui::Spec{spec}}, right{ui::Spec{spec}};
  test::MockPlatform a, b;
  left.resize({300, 60});
  right.resize({120, 60});
  left.activate(a);
  right.activate(b);
  group_test::render(left, {300, 60});
  group_test::render(right, {120, 60});
  NUI_CHECK(visible(left, "Undo") && !visible(left, "More"));
  NUI_CHECK(!visible(right, "Undo") && visible(right, "More"));
  open_more(right, b);
  left.dispatch(test::key(ui::Key::Right), a);
  NUI_CHECK(group_test::focused(left) == "Undo");
  NUI_CHECK(right.overlay_entries().size() == 1 &&
            left.overlay_entries().empty());
  right.dispatch(test::key(ui::Key::Enter), b);
  NUI_CHECK(calls[1] == 1 && right.overlay_entries().empty());
  NUI_CHECK(group_test::focused(left) == "Undo");
  right.resize({300, 60});
  group_test::render(right, {300, 60});
  NUI_CHECK(visible(right, "Undo") && !visible(right, "More"));
}
void editor_and_nested_group_keep_navigation_and_spacer_flex() {
  ui::State<std::string> text{"abc"};
  ui::State<bool> bold{false}, italic{false};
  auto editor = std::make_shared<group_test::Audit>();
  auto last = std::make_shared<group_test::Audit>();
  std::vector<ui::Spec> formatting;
  formatting.push_back(ui::ToggleButton{"Bold", bold}.spec());
  formatting.push_back(ui::ToggleButton{"Italic", italic}.spec());
  std::vector<ui::ToolbarItem> entries;
  entries.push_back(
      {"format",
       ui::ToggleGroup{"Formatting", std::move(formatting)}.spec(),
       {}});
  entries.push_back(
      {"editor",
       group_test::probe(editor, ui::TextInput{"Editor", text}.spec()),
       {}});
  entries.push_back(
      {"space", ui::Flex{ui::Spacer{0.0f, 0.0f}}.grow(1).spec(), {}});
  entries.push_back(
      {"last",
       group_test::probe(last,
                         ui::Button{"Last", {}}.style(button_style()).spec()),
       {}});
  ui::UI tree{ui::Toolbar{"Document", std::move(entries)}.style(bar_style())};
  test::MockPlatform platform;
  tree.resize({900, 80});
  tree.activate(platform);
  group_test::render(tree, {900, 80});
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Italic" && !bold.get() &&
            !italic.get());
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(group_test::focused(tree) == "Bold");
  group_test::click(tree, platform, editor->bounds, false);
  tree.dispatch(test::key(ui::Key::Home), platform);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::text("x"), platform);
  NUI_CHECK(group_test::focused(tree) == "Editor" && text.get() == "axbc");
  NUI_CHECK_NEAR(last->bounds.x + last->bounds.w, 900, .01f);
  NUI_CHECK(!visible(tree, "More"));
}
void suite() {
  overflow_preserves_suffix_order_and_same_application_commands();
  one_tab_stop_last_target_and_readonly_command_behavior();
  focus_and_capture_transfer_to_more_without_remount();
  nonoverflowable_and_whole_group_metadata_are_preserved();
  layout_failure_keeps_last_committed_plan_then_recovers();
  old_menu_and_throwing_command_are_terminal();
  duplicate_keys_are_rejected();
  copied_recipe_has_independent_plan_focus_and_menu();
  editor_and_nested_group_keep_navigation_and_spacer_flex();
}
} // namespace
int main() { return test::run("widget_toolbar", &suite); }
