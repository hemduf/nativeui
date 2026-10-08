#include "test_support.hpp"
#include <nativeui/find_bar.hpp>
namespace {
void navigation_wrap_absence_single_and_close_preserves_query() {
  ui::State<bool> open{true};
  ui::State<std::string> query{"gain"};
  ui::State<std::size_t> matches{3};
  ui::State<std::optional<std::size_t>> current{};
  std::vector<std::size_t> targets;
  ui::UI tree{ui::FindBar{open, query, matches, current}.on_navigate(
      [&](auto n) { targets.push_back(n); })};
  test::MockPlatform platform;
  tree.resize({800, 80});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(current.get() == 0);
  tree.dispatch(test::key(ui::Key::Enter, true), platform);
  NUI_CHECK(current.get() == 2);
  ui::InputEvent next;
  next.type = ui::InputType::Command;
  next.command = ui::Command::FindNext;
  tree.dispatch(next, platform);
  NUI_CHECK(current.get() == 0);
  matches.set(1);
  tree.dispatch(next, platform);
  NUI_CHECK(current.get() == 0 && targets.size() == 4);
  matches.set(0);
  tree.dispatch(next, platform);
  NUI_CHECK(targets.size() == 4 && current.get() == 0);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(!open.get() && query.get() == "gain");
  NUI_CHECK(tree.measure().preferred.w == 0 && tree.measure().preferred.h == 0);
  open.set(true);
  tree.dispatch(test::text("other"), platform);
  NUI_CHECK(query.get() == "other");
}
void observer_count_changes_suppress_stale_callback_and_exception_recovers() {
  ui::State<bool> open{true};
  ui::State<std::string> query{"x"};
  ui::State<std::size_t> matches{3};
  ui::State<std::optional<std::size_t>> current{0};
  int navigations{};
  bool shrink = true, fail{};
  auto subscription = current.observe([&](const auto &) {
    if (shrink)
      matches.set(0);
    if (fail)
      throw std::runtime_error("current observer fault");
  });
  ui::UI tree{ui::FindBar{open, query, matches, current}.on_navigate(
      [&](auto) { ++navigations; })};
  test::MockPlatform platform;
  tree.resize({800, 80});
  tree.activate(platform);
  ui::InputEvent next;
  next.type = ui::InputType::Command;
  next.command = ui::Command::FindNext;
  tree.dispatch(next, platform);
  NUI_CHECK(current.get() == 1 && navigations == 0);
  shrink = false;
  matches.set(3);
  fail = true;
  bool caught{};
  try {
    tree.dispatch(next, platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && current.get() == 2 && navigations == 0);
  fail = false;
  tree.dispatch(next, platform);
  NUI_CHECK(current.get() == 0 && navigations == 1);
}
void suite() {
  navigation_wrap_absence_single_and_close_preserves_query();
  observer_count_changes_suppress_stale_callback_and_exception_recovers();
}
} // namespace
int main() { return test::run("widget_find_bar", &suite); }
