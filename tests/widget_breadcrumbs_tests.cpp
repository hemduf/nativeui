#include "test_support.hpp"
#include <nativeui/breadcrumbs.hpp>

namespace {
using Path = std::vector<ui::BreadcrumbItem>;
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void exact_keys_and_terminal_destination() {
  ui::State<Path> path{
      Path{{"root", "Root"}, {"home", "Home"}, {"leaf", "Destination"}}};
  std::vector<std::string> calls;
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [&](const auto &key) { calls.push_back(key); })};
  test::MockPlatform platform;
  tree.resize({600, 40});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(calls.empty());
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls == std::vector<std::string>{"root"});
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls.back() == "home");
  bool terminal{};
  for (ui::NodeId id = 1; id < 32; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->name == "Destination") {
      terminal = true;
      NUI_CHECK(info->role == ui::SemanticRole::Text && !info->focusable &&
                info->actions.empty());
    }
  }
  NUI_CHECK(terminal);
}
void removed_or_terminal_key_cancels_a_press() {
  ui::State<Path> path{Path{{"root", "Root"}, {"leaf", "Destination"}}};
  int calls{};
  ui::UI tree{
      ui::Breadcrumbs{path}.on_navigate([&](const auto &) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({400, 40});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 15, 20), platform);
  path.set({{"root", "Root"}});
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 15, 20), platform);
  NUI_CHECK(calls == 0 && platform.pointer_capture_end_count == 1);
}
void overflow_is_keyed_and_invalid_updates_keep_the_last_path() {
  Path original{{"root", "Root"},
                {"one", "First"},
                {"two", "Second"},
                {"three", "Third"},
                {"leaf", "Very long Unicode destination"}};
  ui::State<Path> path{original};
  std::vector<std::string> calls;
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [&](const auto &key) { calls.push_back(key); })};
  test::MockPlatform platform;
  tree.resize({100, 40});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == std::vector<std::string>{"root"});
  NUI_CHECK(tree.overlay_entries().empty());
  path.set({{"dup", "One"}, {"dup", "Two"}});
  ui::HeadlessRenderer renderer{{100, 40}, 1};
  NUI_CHECK(renderer.render(tree));
  bool old_leaf{}, diagnostic{};
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (!info)
      continue;
    old_leaf |= info->name == original.back().label;
    diagnostic |=
        info->role == ui::SemanticRole::Group && !info->description.empty();
  }
  NUI_CHECK(old_leaf && diagnostic);
  bool rejected{};
  try {
    (void)ui::Breadcrumbs{path}.spec();
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void callback_failure_disarms_and_allows_another_activation() {
  ui::State<Path> path{Path{{"root", "Root"}, {"leaf", "Destination"}}};
  bool fail = true;
  int calls{};
  ui::UI tree{ui::Breadcrumbs{path}.on_navigate([&](const auto &) {
    ++calls;
    if (fail)
      throw std::runtime_error("navigate");
  })};
  test::MockPlatform platform;
  tree.resize({400, 40});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 15, 20), platform);
  bool caught{};
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 15, 20), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1 && platform.pointer_capture_end_count == 1);
  ui::HeadlessRenderer renderer{{400, 40}, 1};
  NUI_CHECK(renderer.render(tree) && calls == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls == 2);
}
void suite() {
  exact_keys_and_terminal_destination();
  removed_or_terminal_key_cancels_a_press();
  overflow_is_keyed_and_invalid_updates_keep_the_last_path();
  callback_failure_disarms_and_allows_another_activation();
}
} // namespace
int main() { return test::run("widget_breadcrumbs", &suite); }
