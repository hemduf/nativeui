#include "test_support.hpp"
#include <nativeui/calendar.hpp>
namespace {
auto day(int year, unsigned month, unsigned date) {
  return std::chrono::sys_days{std::chrono::year{year} /
                               std::chrono::month{month} /
                               std::chrono::day{date}};
}
void suite() {
  using V = ui::Calendar::Value;
  ui::State<V> value{day(2024, 1, 31)};
  int calls{};
  ui::UI tree{ui::Calendar{"Day", value}.on_change([&](V v) {
    NUI_CHECK(v == value.get());
    ++calls;
  })};
  test::MockPlatform platform;
  tree.resize({340, 320});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{340, 320}};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.structural_diagnostic().empty());
  // The public semantic projection includes both navigation buttons and all
  // 42 dates before any interaction or retained reconciliation takes place.
  int buttons{};
  for (ui::NodeId id = 1; id <= 46; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->role == ui::SemanticRole::Button)
      ++buttons;
  }
  NUI_CHECK(buttons == 44);
  tree.dispatch(test::key(ui::Key::PageDown), platform);
  NUI_CHECK(value.get() == day(2024, 2, 29) && calls == 1);
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.structural_diagnostic().empty());
  value.set(day(2100, 1, 31));
  tree.dispatch(test::key(ui::Key::PageDown), platform);
  NUI_CHECK(value.get() == day(2100, 2, 28) && calls == 2);
  ui::State<V> draft{day(2026, 10, 4)};
  ui::UI separate{
      ui::Calendar{"Draft", draft}.commit_on_navigation(false).range(
          day(2026, 10, 1), day(2026, 10, 5))};
  separate.resize({340, 320});
  separate.activate(platform);
  separate.dispatch(test::key(ui::Key::Right), platform);
  separate.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(draft.get() == day(2026, 10, 4));
  separate.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(draft.get() == day(2026, 10, 5));
  draft.set(day(2026, 10, 2));
  separate.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(draft.get() == day(2026, 10, 2));
  bool rejected{};
  try {
    (void)ui::Calendar{"Bad", draft}
        .range(day(2026, 2, 1), day(2026, 1, 1))
        .spec();
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
} // namespace
int main() { return test::run("widget_calendar", &suite); }
