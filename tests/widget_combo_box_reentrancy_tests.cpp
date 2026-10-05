#include "test_support.hpp"
#include <nativeui/combo_box.hpp>
namespace {
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void a_fresh_choice_after_external_selection_remains_available() {
  ui::State<int> value{1};
  ui::UI tree{ui::ComboBox<int>{
      value, {{1, "One", true}, {2, "Two", true}, {3, "Three", true}}}};
  test::MockPlatform platform;
  tree.resize({440, 280});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::End), platform);
  value.set(2);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == 3 && tree.overlay_entries().empty());
}
struct EqualityChoice {
  int id{};
  inline static bool armed{};
  inline static std::function<void()> boundary;
  friend bool operator==(const EqualityChoice &a, const EqualityChoice &b) {
    if (armed && a.id == 3 && b.id == 1) {
      armed = false;
      auto callback = boundary;
      if (callback)
        callback();
    }
    return a.id == b.id;
  }
};
void late_state_equality_respects_read_only() {
  ui::State<EqualityChoice> value{EqualityChoice{1}};
  ui::State<bool> locked{false};
  ui::UI tree{ui::ReadOnly{
      locked, ui::ComboBox<EqualityChoice>{
                  value, {{{1}, "One", true}, {{3}, "Three", true}}}}};
  test::MockPlatform platform;
  tree.resize({440, 280});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::End), platform);
  auto lock = locked.binding();
  EqualityChoice::boundary = [lock]() mutable { lock.set(true); };
  EqualityChoice::armed = true;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  EqualityChoice::armed = false;
  EqualityChoice::boundary = {};
  NUI_CHECK(locked.get() && value.get().id == 1 &&
            tree.overlay_entries().empty());
}
void suite() {
  a_fresh_choice_after_external_selection_remains_available();
  late_state_equality_respects_read_only();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "equality")
    return test::run("combo_late_equality",
                     &late_state_equality_respects_read_only);
  if (argc == 2 && std::string_view{argv[1]} == "external")
    return test::run(
        "combo_external_choice",
        &a_fresh_choice_after_external_selection_remains_available);
  return test::run("widget_combo_box_reentrancy", &suite);
}
