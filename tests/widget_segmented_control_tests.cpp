#include "group_test_support.hpp"
#include <nativeui/segmented_control.hpp>
namespace {
std::vector<ui::SegmentOption<int>> options() {
  return {{1, "List", true}, {2, "Grid", true}, {3, "Other", true}};
}
void navigation_and_activation_publish_only_real_changes() {
  ui::State<int> value{1};
  int changes{};
  auto subscription = value.observe([&](int) { ++changes; });
  ui::UI tree{ui::Column{ui::Button{"Before", {}},
                         ui::SegmentedControl{"View", value, options()},
                         ui::Button{"After", {}}}};
  test::MockPlatform platform;
  tree.resize({640, 180});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "List");
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == 1 && changes == 0);
  group_test::release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 2 && changes == 1 &&
            group_test::focused(tree) == "Grid");
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(value.get() == 3 && changes == 2);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 1 && changes == 3);
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(value.get() == 3);
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(value.get() == 1);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "After");
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  NUI_CHECK(group_test::focused(tree) == "List");
  const auto first = group_test::info(tree, "List"),
             second = group_test::info(tree, "Grid");
  NUI_CHECK(first && second && first->role == ui::SemanticRole::RadioButton &&
            first->selected && !second->selected);
  NUI_CHECK(first->checked == ui::SemanticCheckedState::Checked &&
            first->supports(ui::SemanticAction::Select));
}
void unknown_disabled_empty_and_duplicates_preserve_model() {
  ui::State<int> value{99};
  int changes{};
  auto subscription = value.observe([&](int) { ++changes; });
  ui::UI tree{ui::SegmentedControl{"View", value,
                                   std::vector<ui::SegmentOption<int>>{
                                       {1, "List", true}, {2, "Grid", false}}}};
  test::MockPlatform platform;
  tree.resize({260, 60});
  tree.activate(platform);
  group_test::render(tree, {260, 60});
  NUI_CHECK(value.get() == 99 && changes == 0 &&
            !group_test::info(tree, "List")->selected);
  value.set(2);
  group_test::render(tree, {260, 60});
  auto disabled = group_test::info(tree, "Grid");
  NUI_CHECK(disabled && disabled->selected && !disabled->enabled);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 1);
  ui::UI empty{ui::SegmentedControl{"Empty", value,
                                    std::vector<ui::SegmentOption<int>>{}}};
  empty.resize({100, 40});
  empty.activate(platform);
  NUI_CHECK(group_test::focused(empty).empty() && value.get() == 1);
  bool rejected{};
  try {
    auto bad = ui::SegmentedControl{"Bad", value,
                                    std::vector<ui::SegmentOption<int>>{
                                        {1, "a", true}, {1, "b", true}}}
                   .spec();
    (void)bad;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void external_selection_changes_entry_without_stealing_focus() {
  ui::State<int> value{1};
  ui::State<bool> readonly{true};
  ui::UI tree{ui::Column{
      ui::Button{"Before", {}},
      ui::ReadOnly{readonly, ui::SegmentedControl{"View", value, options()}},
      ui::Button{"After", {}}}};
  test::MockPlatform platform;
  tree.resize({640, 180});
  tree.activate(platform);
  value.set(3);
  group_test::render(tree);
  NUI_CHECK(group_test::focused(tree) == "Before");
  tree.dispatch(test::key(ui::Key::Tab), platform);
  NUI_CHECK(group_test::focused(tree) == "Other");
  tree.dispatch(test::key(ui::Key::Left), platform);
  NUI_CHECK(group_test::focused(tree) == "Grid" && value.get() == 3);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == 3);
  auto info = group_test::info(tree, "Grid");
  NUI_CHECK(info && info->read_only &&
            !info->supports(ui::SemanticAction::Select));
}
void observer_throw_and_subtree_removal_restore_interactions() {
  ui::State<int> value{1};
  bool fail{true};
  auto subscription = value.observe([&](int) {
    if (fail)
      throw std::runtime_error("segmented observer");
  });
  ui::UI tree{ui::SegmentedControl{"View", value, options()}};
  test::MockPlatform platform;
  tree.resize({340, 70});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Right), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == 2);
  fail = false;
  group_test::render(tree, {340, 70});
  NUI_CHECK(group_test::info(tree, "Grid")->selected);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 3);
  ui::State<bool> shown{true};
  int removals{};
  auto remove = value.observe([&](int) {
    ++removals;
    shown.set(false);
  });
  ui::UI removed{
      ui::If{shown, ui::SegmentedControl{"Transient", value, options()}}};
  removed.resize({340, 70});
  removed.activate(platform);
  group_test::render(removed, {340, 70});
  removed.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(value.get() == 1 && removals == 1);
  group_test::render(removed, {340, 70});
  NUI_CHECK(!group_test::node(removed, "List"));
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
struct UserValue {
  int code{};
  std::shared_ptr<std::function<void()>> on_copy, on_equal;
  UserValue() = default;
  UserValue(int value, std::shared_ptr<std::function<void()>> copy = {},
            std::shared_ptr<std::function<void()>> equal = {})
      : code(value), on_copy(std::move(copy)), on_equal(std::move(equal)) {}
  UserValue(const UserValue &other)
      : code(other.code), on_copy(other.on_copy), on_equal(other.on_equal) {
    if (on_copy && *on_copy)
      (*on_copy)();
  }
  UserValue(UserValue &&) = default;
  UserValue &operator=(const UserValue &) = default;
  UserValue &operator=(UserValue &&) = default;
  bool operator==(const UserValue &other) const {
    if (on_equal && *on_equal)
      (*on_equal)();
    return code == other.code;
  }
};
void arbitrary_type_copy_and_equality_boundaries_are_guarded() {
  auto copy = std::make_shared<std::function<void()>>(),
       equal = std::make_shared<std::function<void()>>();
  ui::State<UserValue> value{UserValue{1, copy, equal}};
  ui::State<bool> readonly{false};
  ui::UI tree{ui::ReadOnly{
      readonly,
      ui::SegmentedControl{"Generic", value,
                           std::vector<ui::SegmentOption<UserValue>>{
                               {UserValue{1, copy, equal}, "One", true},
                               {UserValue{2, copy, equal}, "Two", true}}}}};
  test::MockPlatform platform;
  tree.resize({300, 70});
  tree.activate(platform);
  bool armed{true};
  *equal = [&] {
    if (armed) {
      armed = false;
      readonly.set(true);
    }
  };
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(!armed && value.get().code == 1 && readonly.get());
  *equal = {};
  readonly.set(false);
  group_test::render(tree, {300, 70});
  armed = true;
  *copy = [&] {
    if (armed) {
      armed = false;
      value.set(UserValue{99, copy, equal});
    }
  };
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!armed && value.get().code == 99);
  *copy = {};
  group_test::release(tree, platform, ui::Key::Enter);
  group_test::render(tree, {300, 70});
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(value.get().code == 1);
}
void expired_source_is_readable_and_inert() {
  auto owner = std::make_unique<ui::State<int>>(1);
  auto binding = owner->binding();
  ui::UI tree{ui::SegmentedControl{"View", binding, options()}};
  test::MockPlatform platform;
  tree.resize({340, 70});
  tree.activate(platform);
  owner.reset();
  group_test::render(tree, {340, 70});
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(binding.get() == 1 && !binding.valid());
  const auto info = group_test::info(tree, "Grid");
  NUI_CHECK(info && info->read_only &&
            !info->supports(ui::SemanticAction::Select));
}
void suite() {
  navigation_and_activation_publish_only_real_changes();
  unknown_disabled_empty_and_duplicates_preserve_model();
  external_selection_changes_entry_without_stealing_focus();
  observer_throw_and_subtree_removal_restore_interactions();
  arbitrary_type_copy_and_equality_boundaries_are_guarded();
  expired_source_is_readable_and_inert();
}
} // namespace
int main() { return test::run("widget_segmented_control", &suite); }
