#include "test_support.hpp"
#include <nativeui/radio_button.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace {
struct Choice {
  std::string name;
  friend bool operator==(const Choice &, const Choice &) = default;
};
void custom_value_and_group_lifetime() {
  ui::State<Choice> selected{Choice{"small"}};
  auto tree = [&] {
    ui::RadioGroup<Choice> group{selected};
    return ui::UI{ui::Column{ui::RadioButton{group, Choice{"small"}, "Small"},
                             ui::RadioButton{group, Choice{"large"}, "Large"}}};
  }();
  test::MockPlatform platform;
  tree.resize({200.0f, 100.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(selected.get() == Choice{"large"});
  int changes{};
  auto observer = selected.observe([&](const Choice &) { ++changes; });
  tree.dispatch(test::key(ui::Key::Space), platform);
  auto up = test::key(ui::Key::Space);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  NUI_CHECK(changes == 0);
  ui::HeadlessRenderer renderer{{200.0f, 100.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
}
void duplicate_values() {
  ui::State<int> selected{1};
  ui::RadioGroup<int> group{selected};
  auto first = ui::RadioButton{group, 1, "Repeated label"}.spec();
  auto second = ui::RadioButton{group, 2, "Repeated label"}.spec();
  (void)first;
  (void)second;
  bool rejected = false;
  try {
    auto duplicate = ui::RadioButton{group, 1, "Other label"}.spec();
    (void)duplicate;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void selection_throw_recovers() {
  ui::State<int> selected{1};
  bool throw_next = true;
  auto observer = selected.observe([&](int) {
    if (throw_next)
      throw std::runtime_error("radio observer fault");
  });
  ui::RadioGroup<int> group{selected};
  ui::UI tree{ui::Column{ui::RadioButton{group, 1, "One"},
                         ui::RadioButton{group, 2, "Two"}}};
  test::MockPlatform platform;
  tree.resize({200.0f, 100.0f});
  tree.activate(platform);
  bool caught = false;
  try {
    tree.dispatch(test::key(ui::Key::Down), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && selected.get() == 2);
  throw_next = false;
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(selected.get() == 1);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void equality_failure_does_not_retain_keyboard_press() {
  for (const bool deactivate : {false, true}) {
    bool fail = false;
    int selections{};
    auto group = std::make_shared<ui::detail::RadioGroupToken>();
    ui::detail::RadioButtonComponent component{
        group,
        "Option",
        [&] {
          if (fail)
            throw std::runtime_error("injected option equality");
          return false;
        },
        [&] { ++selections; },
        {},
        {}};
    test::MockPlatform platform;
    ui::InputContext input{
        {0.0f, 0.0f, 200.0f, 40.0f}, platform, [] {}, [] {}, [] {}, [] {}};
    ui::FocusContext focus{{0.0f, 0.0f, 200.0f, 40.0f}, platform, [] {}, [] {}};
    ui::LifecycleContext lifecycle{
        1, {0.0f, 0.0f, 200.0f, 40.0f}, [] {}, [] {}};
    component.focus_changed(true, focus);
    (void)component.input(test::key(ui::Key::Space), input);
    fail = true;
    try {
      if (deactivate)
        component.deactivate(lifecycle);
      else
        component.focus_changed(false, focus);
    } catch (const std::runtime_error &) {
    }
    fail = false;
    auto up = test::key(ui::Key::Space);
    up.type = ui::InputType::KeyUp;
    (void)component.input(up, input);
    NUI_CHECK(selections == 0);
  }
}
void equality_failure_blur_releases_pointer() {
  bool fail = false;
  int captures{};
  int releases{};
  ui::detail::RadioButtonComponent component{
      std::make_shared<ui::detail::RadioGroupToken>(),
      "Option",
      [&] {
        if (fail)
          throw std::runtime_error("injected option equality");
        return false;
      },
      [] {},
      {},
      {}};
  test::MockPlatform platform;
  ui::InputContext input{
      {0.0f, 0.0f, 200.0f, 40.0f}, platform,           [] {}, [] {},
      [&] { ++captures; },         [&] { ++releases; }};
  ui::FocusContext focus{{0.0f, 0.0f, 200.0f, 40.0f}, platform, [] {}, [] {}};
  component.focus_changed(true, focus);
  (void)component.input(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f),
                        input);
  fail = true;
  try {
    component.focus_changed(false, focus);
  } catch (const std::runtime_error &) {
  }
  NUI_CHECK(captures == 1 && releases == 1);
}
void home_end_select_group_boundaries() {
  ui::State<int> selected{2};
  ui::RadioGroup<int> group{selected};
  ui::UI tree{ui::Column{ui::RadioButton{group, 1, "One"},
                         ui::RadioButton{group, 2, "Two"},
                         ui::RadioButton{group, 3, "Three"}}};
  test::MockPlatform platform;
  tree.resize({200.0f, 140.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(selected.get() == 3);
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(selected.get() == 1);
}
void suite() {
  equality_failure_does_not_retain_keyboard_press();
  equality_failure_blur_releases_pointer();
  home_end_select_group_boundaries();
  custom_value_and_group_lifetime();
  duplicate_values();
  selection_throw_recovers();
}
} // namespace
int main() { return test::run("widget_radio_extraction", &suite); }
