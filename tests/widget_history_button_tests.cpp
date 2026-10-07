#include "test_support.hpp"
#include <nativeui/history_button.hpp>

namespace {
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
ui::InputEvent context_menu() {
  ui::InputEvent event;
  event.type = ui::InputType::ContextMenu;
  event.position = {10, 15};
  return event;
}
void directions_and_release_activation() {
  ui::State<bool> can{true};
  std::vector<int> calls;
  auto navigate = [&](int steps) { calls.push_back(steps); };
  ui::UI back{ui::HistoryButton{ui::HistoryDirection::Backward, can, navigate}};
  ui::UI forward{
      ui::HistoryButton{ui::HistoryDirection::Forward, can, navigate}};
  test::MockPlatform platform;
  back.resize({80, 40});
  forward.resize({80, 40});
  back.activate(platform);
  back.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls.empty());
  release(back, platform, ui::Key::Enter);
  forward.activate(platform);
  forward.dispatch(test::key(ui::Key::Space), platform);
  release(forward, platform, ui::Key::Space);
  NUI_CHECK(calls == std::vector<int>({-1, 1}));
}
void menu_cap_and_key_rebase() {
  using Entries = std::vector<ui::HistoryEntry>;
  ui::State<bool> can{true};
  ui::State<Entries> entries{
      Entries{{"a", "Same"}, {"b", "Same"}, {"c", "Third"}}};
  int last{}, calls{};
  ui::UI tree{ui::HistoryButton{ui::HistoryDirection::Backward, can,
                                [&](int steps) {
                                  last = steps;
                                  ++calls;
                                }}
                  .entries(entries)
                  .maximum_menu_entries(2)};
  test::MockPlatform platform;
  tree.resize({480, 320});
  tree.activate(platform);
  tree.dispatch(context_menu(), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::End), platform);
  entries.set({{"b", "Same"}, {"a", "Same"}, {"c", "Third"}});
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(last == -1 && calls == 1 && tree.overlay_entries().empty());
  release(tree, platform, ui::Key::Enter);
  tree.dispatch(context_menu(), platform);
  tree.dispatch(test::key(ui::Key::End), platform);
  entries.set({{"b", "Same"}, {"c", "Third"}});
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1 && tree.overlay_entries().empty());
  ui::UI no_menu{ui::HistoryButton{ui::HistoryDirection::Backward, can,
                                   [&](int) { ++calls; }}
                     .entries(entries)
                     .maximum_menu_entries(0)};
  no_menu.resize({480, 320});
  no_menu.activate(platform);
  no_menu.dispatch(context_menu(), platform);
  NUI_CHECK(no_menu.overlay_entries().empty());
}
void read_only_can_and_lifetime_gate_navigation() {
  ui::State<bool> can{true}, locked{false};
  int calls{};
  ui::UI tree{
      ui::ReadOnly{locked, ui::HistoryButton{ui::HistoryDirection::Backward,
                                             can, [&](int) { ++calls; }}}};
  test::MockPlatform platform;
  tree.resize({80, 40});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 15), platform);
  can.set(false);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 15), platform);
  NUI_CHECK(calls == 0 && platform.pointer_capture_end_count == 1);
  can.set(true);
  locked.set(true);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  release(tree, platform, ui::Key::Enter);
  NUI_CHECK(calls == 0);
  auto owner = std::make_unique<ui::State<bool>>(true);
  ui::UI expired{ui::HistoryButton{ui::HistoryDirection::Forward, *owner,
                                   [&](int) { ++calls; }}};
  expired.resize({80, 40});
  expired.activate(platform);
  owner.reset();
  expired.dispatch(test::key(ui::Key::Space), platform);
  release(expired, platform, ui::Key::Space);
  NUI_CHECK(calls == 0);
}
void navigation_failure_is_not_replayed() {
  ui::State<bool> can{true};
  bool fail = true;
  int calls{};
  ui::UI tree{ui::HistoryButton{ui::HistoryDirection::Backward, can, [&](int) {
                                  ++calls;
                                  if (fail)
                                    throw std::runtime_error("navigate");
                                }}};
  test::MockPlatform platform;
  tree.resize({80, 40});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  bool caught{};
  try {
    release(tree, platform, ui::Key::Space);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1);
  ui::HeadlessRenderer renderer{{80, 40}, 1};
  NUI_CHECK(renderer.render(tree) && calls == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls == 2);
}
void suite() {
  directions_and_release_activation();
  menu_cap_and_key_rebase();
  read_only_can_and_lifetime_gate_navigation();
  navigation_failure_is_not_replayed();
}
} // namespace
int main() { return test::run("widget_history_button", &suite); }
