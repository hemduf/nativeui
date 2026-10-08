#include "test_support.hpp"
#include <chrono>
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/popup_menu.hpp>
namespace {
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
ui::PopupMenuItem branch(std::string key, std::string label,
                         std::vector<ui::PopupMenuItem> children) {
  ui::PopupMenuItem result;
  result.key = std::move(key);
  result.label = std::move(label);
  result.children = std::move(children);
  return result;
}
std::optional<ui::SemanticInfo> named(const ui::UI &tree, ui::SemanticRole role,
                                      std::string_view name) {
  for (ui::NodeId id = 1; id < 4096; ++id) {
    auto info = tree.component_semantics(id);
    if (info && info->role == role && info->name == name)
      return info;
  }
  return {};
}
void provider_is_an_owned_snapshot_until_the_next_open() {
  int value = 41, providers{}, accepted{};
  ui::UI tree{ui::PopupMenu{
      "Actions", [&] {
        ++providers;
        return std::vector<ui::PopupMenuItem>{ui::PopupMenuItem::action(
            "Value", [&, owned = value] { accepted = owned; })};
      }}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  NUI_CHECK(providers == 0);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(providers == 1 && accepted == 0);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(accepted == 0 && tree.overlay_entries().size() == 1);
  value = 42;
  ui::HeadlessRenderer renderer{{400, 300}, 1};
  NUI_CHECK(renderer.render(tree) && providers == 1);
  release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(accepted == 41 && tree.overlay_entries().empty());
  release(tree, platform, ui::Key::Enter);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(providers == 2 && accepted == 42 && tree.overlay_entries().empty());
}
void navigation_skips_inactive_rows_and_traverses_submenus() {
  int picked{};
  auto root = branch("root", "More",
                     {ui::PopupMenuItem::separator(),
                      ui::PopupMenuItem::action(
                          "Disabled", [] {}, false),
                      ui::PopupMenuItem::action("Pick", [&] { ++picked; })});
  ui::UI tree{ui::PopupMenu{
      "Actions", std::vector<ui::PopupMenuItem>{
                     ui::PopupMenuItem::separator(),
                     ui::PopupMenuItem::action(
                         "Disabled", [] {}, false),
                     ui::PopupMenuItem::action("No callback", {}), root}}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(tree.overlay_entries().size() == 2 && picked == 0);
  tree.dispatch(test::key(ui::Key::Left), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Home), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(picked == 1 && tree.overlay_entries().empty());
}
void checked_shortcuts_and_expanded_state_are_published() {
  auto item = ui::PopupMenuItem::action("Marked", [] {});
  item.key = "marked";
  item.checked = false;
  item.shortcut_label = "Ctrl+M";
  ui::UI tree{ui::PopupMenu{"Actions", std::vector<ui::PopupMenuItem>{item}}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  auto anchor = named(tree, ui::SemanticRole::Button, "Actions");
  NUI_CHECK(anchor && anchor->expanded == ui::SemanticExpandedState::Collapsed);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  anchor = named(tree, ui::SemanticRole::Button, "Actions");
  auto marked = named(tree, ui::SemanticRole::MenuItem, "Marked");
  NUI_CHECK(anchor && anchor->expanded == ui::SemanticExpandedState::Expanded);
  NUI_CHECK(marked && marked->checked == ui::SemanticCheckedState::Unchecked &&
            marked->description.find("Ctrl+M") != std::string::npos);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  anchor = named(tree, ui::SemanticRole::Button, "Actions");
  NUI_CHECK(anchor && anchor->expanded == ui::SemanticExpandedState::Collapsed);
}
void invalid_trees_are_rejected_before_handle_publication() {
  auto a = ui::PopupMenuItem::action("A", [] {});
  a.key = "duplicate";
  auto b = a;
  int mode{};
  ui::UI tree{ui::PopupMenu{"Actions", [&] {
                              if (mode == 0)
                                return std::vector<ui::PopupMenuItem>{a, b};
                              if (mode == 1) {
                                auto item = branch("root", "More", {a});
                                item.callback = [] {};
                                return std::vector<ui::PopupMenuItem>{item};
                              }
                              if (mode == 2) {
                                auto item = a;
                                for (int level = 0; level < 17; ++level)
                                  item = branch("nested", "More", {item});
                                return std::vector<ui::PopupMenuItem>{item};
                              }
                              return std::vector<ui::PopupMenuItem>{a};
                            }}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  for (mode = 0; mode < 3; ++mode) {
    bool rejected{};
    try {
      tree.dispatch(test::key(ui::Key::Down), platform);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    NUI_CHECK(rejected && tree.overlay_entries().empty());
    release(tree, platform, ui::Key::Down);
  }
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
}
void long_panels_remain_in_the_logical_viewport() {
  for (float scale : {1.f, 2.f}) {
    std::vector<ui::PopupMenuItem> items;
    int picked = -1;
    for (int index = 0; index < 100; ++index)
      items.push_back(ui::PopupMenuItem::action(
          "Entry " + std::to_string(index), [&, index] { picked = index; }));
    ui::UI tree{ui::PopupMenu{"Actions", items}};
    test::MockPlatform platform;
    tree.resize({180, 140});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Down), platform);
    release(tree, platform, ui::Key::Down);
    ui::HeadlessRenderer renderer{{180, 140}, scale};
    NUI_CHECK(renderer.render(tree));
    const auto entries = tree.overlay_entries();
    NUI_CHECK(entries.size() == 1 && entries[0].resolved &&
              entries[0].bounds.x >= 0 && entries[0].bounds.y >= 0 &&
              entries[0].bounds.x + entries[0].bounds.w <= 180.01f &&
              entries[0].bounds.y + entries[0].bounds.h <= 140.01f);
    auto wheel =
        test::pointer(ui::InputType::PointerWheel, entries[0].bounds.x + 10,
                      entries[0].bounds.y + 10);
    wheel.delta = {0, -90};
    tree.dispatch(wheel, platform);
    tree.dispatch(test::key(ui::Key::End), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(picked == 99 && tree.overlay_entries().empty());
  }
}
void hover_timer_is_cancellable_and_owned_by_the_session() {
  auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
  ui::detail::DispatcherOwner owner{{}, clock};
  test::MockPlatform platform;
  platform.dispatcher_value = owner.dispatcher();
  ui::UI tree{ui::PopupMenu{
      "Actions",
      std::vector<ui::PopupMenuItem>{
          branch("more", "More", {ui::PopupMenuItem::action("Pick", [] {})})}}};
  tree.resize({400, 300});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  const auto bounds = tree.overlay_entries()[0].bounds;
  tree.dispatch(test::pointer(ui::InputType::PointerMove, bounds.x + 10,
                              bounds.y + bounds.h * .5f),
                platform);
  clock->advance(std::chrono::milliseconds{199});
  owner.checkpoint();
  ui::InputEvent tick;
  tick.type = ui::InputType::Tick;
  tree.dispatch(tick, platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  clock->advance(std::chrono::milliseconds{1});
  owner.checkpoint();
  tree.dispatch(tick, platform);
  NUI_CHECK(tree.overlay_entries().size() == 2);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  clock->advance(std::chrono::milliseconds{500});
  owner.checkpoint();
  tree.dispatch(tick, platform);
  NUI_CHECK(tree.overlay_entries().empty() && owner.active_timer_count() == 0);
}
void outside_dismissal_closes_the_whole_submenu_session() {
  auto nested =
      branch("root", "More", {ui::PopupMenuItem::action("Pick", [] {})});
  ui::UI tree{ui::PopupMenu{"Actions", std::vector<ui::PopupMenuItem>{nested}}};
  test::MockPlatform platform;
  tree.resize({600, 400});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(tree.overlay_entries().size() == 2);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 590, 390), platform);
  NUI_CHECK(tree.overlay_entries().empty());
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void suite() {
  provider_is_an_owned_snapshot_until_the_next_open();
  navigation_skips_inactive_rows_and_traverses_submenus();
  checked_shortcuts_and_expanded_state_are_published();
  invalid_trees_are_rejected_before_handle_publication();
  long_panels_remain_in_the_logical_viewport();
  hover_timer_is_cancellable_and_owned_by_the_session();
  outside_dismissal_closes_the_whole_submenu_session();
}
} // namespace
int main() { return test::run("widget_popup_menu", &suite); }
