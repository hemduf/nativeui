#include "test_support.hpp"
#include <nativeui/autocomplete.hpp>
#include <nativeui/context_menu.hpp>
namespace {
struct ChildState {
  int clicks{}, contexts{}, focus_in{};
  ui::Rect bounds{};
};
class Child final : public ui::Component {
public:
  explicit Child(std::shared_ptr<ChildState> state)
      : state_(std::move(state)) {}
  bool focusable() const noexcept override { return true; }
  ui::Size measure(const std::vector<ui::ChildMetrics> &) const override {
    return {220, 120};
  }
  void layout_committed(ui::Rect, ui::Rect current) noexcept override {
    state_->bounds = current;
  }
  void focus_changed(bool focused, ui::FocusContext &) override {
    if (focused)
      ++state_->focus_in;
  }
  ui::EventResult input(const ui::InputEvent &event,
                        ui::InputContext &) override {
    if (event.type == ui::InputType::ContextMenu)
      ++state_->contexts;
    if (event.type == ui::InputType::PointerDown)
      ++state_->clicks;
    return ui::EventResult::Ignored;
  }
  ui::SemanticInfo semantics() const override {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Button;
    info.name = "Document";
    info.focusable = true;
    return info;
  }
  void paint(ui::PaintContext &) const override {}

private:
  std::shared_ptr<ChildState> state_;
};
ui::Spec child(std::shared_ptr<ChildState> state) {
  return ui::Spec{[state] { return std::make_unique<Child>(state); }, {}};
}
void close(ui::UI &tree, test::MockPlatform &platform) {
  tree.dispatch(test::key(ui::Key::Escape), platform);
}
void secondary_request_opens_at_the_logical_point_without_primary_action() {
  auto state = std::make_shared<ChildState>();
  int calls{}, providers{};
  ui::UI tree{ui::ContextMenu{child(state), [&] {
                                ++providers;
                                return std::vector<ui::PopupMenuItem>{
                                    ui::PopupMenuItem::action(
                                        "Copy", [&] { ++calls; })};
                              }}};
  test::MockPlatform platform;
  tree.resize({500, 350});
  tree.activate(platform);
  const auto before = tree.measure();
  NUI_CHECK(providers == 0 && state->focus_in == 1);
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 170, 90), platform);
  NUI_CHECK(providers == 1 && state->contexts == 1 && state->clicks == 0 &&
            calls == 0 && platform.pointer_capture_begin_count == 0);
  const auto entries = tree.overlay_entries();
  NUI_CHECK(entries.size() == 1 && entries[0].resolved);
  NUI_CHECK_NEAR(entries[0].bounds.x, 170.f, .01f);
  NUI_CHECK_NEAR(entries[0].bounds.y, 90.f, .01f);
  const auto after = tree.measure();
  NUI_CHECK(before.preferred.w == after.preferred.w &&
            before.preferred.h == after.preferred.h &&
            before.minimum.w == after.minimum.w &&
            before.minimum.h == after.minimum.h);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1 && tree.overlay_entries().empty());
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 20, 20), platform);
  NUI_CHECK(state->clicks == 1 && providers == 1);
}
void keyboard_menu_and_shift_f10_share_the_normalized_request() {
  auto state = std::make_shared<ChildState>();
  int providers{};
  ui::UI tree{ui::ContextMenu{child(state), [&] {
                                ++providers;
                                return std::vector<ui::PopupMenuItem>{};
                              }}};
  test::MockPlatform platform;
  tree.resize({500, 350});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::F10), platform);
  NUI_CHECK(providers == 0 && tree.overlay_entries().empty());
  tree.dispatch(test::key(ui::Key::Menu), platform);
  NUI_CHECK(providers == 1 && tree.overlay_entries().size() == 1);
  close(tree, platform);
  auto up = test::key(ui::Key::Menu);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::F10, true), platform);
  NUI_CHECK(providers == 2 && tree.overlay_entries().size() == 1);
}
void nearest_provider_owns_even_an_empty_menu_and_null_provider_bubbles() {
  auto state = std::make_shared<ChildState>();
  int inner{}, outer{};
  ui::UI nested{
      ui::ContextMenu{ui::ContextMenu{child(state),
                                      [&] {
                                        ++inner;
                                        return std::vector<ui::PopupMenuItem>{};
                                      }}
                          .spec(),
                      [&] {
                        ++outer;
                        return std::vector<ui::PopupMenuItem>{
                            ui::PopupMenuItem::action("Outer", [] {})};
                      }}};
  test::MockPlatform platform;
  nested.resize({500, 350});
  nested.activate(platform);
  nested.dispatch(test::pointer(ui::InputType::ContextMenu, 20, 20), platform);
  NUI_CHECK(inner == 1 && outer == 0 && nested.overlay_entries().size() == 1);
  close(nested, platform);
  ui::UI fallback{ui::ContextMenu{
      ui::ContextMenu{child(state), ui::PopupMenu::ItemsProvider{}}.spec(),
      [&] {
        ++outer;
        return std::vector<ui::PopupMenuItem>{};
      }}};
  fallback.resize({500, 350});
  fallback.activate(platform);
  fallback.dispatch(test::pointer(ui::InputType::ContextMenu, 20, 20),
                    platform);
  NUI_CHECK(inner == 1 && outer == 1 && fallback.overlay_entries().size() == 1);
}
void child_semantics_are_preserved_and_no_synthetic_activate_is_advertised() {
  auto state = std::make_shared<ChildState>();
  ui::UI tree{ui::ContextMenu{child(state), std::vector<ui::PopupMenuItem>{}}};
  bool found{};
  for (ui::NodeId id = 1; id < 64; ++id) {
    auto info = tree.component_semantics(id);
    if (!info)
      continue;
    if (info->role == ui::SemanticRole::Button && info->name == "Document")
      found = true;
    if (info->role == ui::SemanticRole::Group)
      NUI_CHECK(!info->supports(ui::SemanticAction::Activate));
  }
  NUI_CHECK(found);
}
void secondary_request_on_a_sibling_does_not_require_existing_focus() {
  auto state = std::make_shared<ChildState>();
  int providers{};
  ui::UI tree{ui::Row{ui::Button{"Other", [] {}},
                      ui::ContextMenu{child(state), [&] {
                                        ++providers;
                                        return std::vector<ui::PopupMenuItem>{};
                                      }}}};
  test::MockPlatform platform;
  tree.resize({600, 300});
  tree.activate(platform);
  NUI_CHECK(state->focus_in == 0);
  const auto target = state->bounds;
  NUI_CHECK(target.w > 0 && target.h > 0);
  tree.dispatch(
      test::pointer(ui::InputType::ContextMenu, target.x + 5, target.y + 5),
      platform);
  NUI_CHECK(providers == 1 && state->clicks == 0 &&
            tree.overlay_entries().size() == 1);
}
void an_idle_legacy_overlay_source_does_not_mask_the_accepting_parent() {
  ui::State<std::string> query{std::string{}};
  int providers{};
  ui::UI tree{ui::ContextMenu{
      ui::Autocomplete{"Query", query, std::vector<std::string>{"Paris"}}
          .spec(),
      [&] {
        ++providers;
        return std::vector<ui::PopupMenuItem>{
            ui::PopupMenuItem::action("Command", [] {})};
      }}};
  test::MockPlatform platform;
  tree.resize({500, 300});
  tree.activate(platform);
  NUI_CHECK(tree.overlay_entries().empty());
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 20, 20), platform);
  NUI_CHECK(providers == 1 && tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(query.get().empty() && tree.overlay_entries().empty());
}
void suite() {
  secondary_request_opens_at_the_logical_point_without_primary_action();
  keyboard_menu_and_shift_f10_share_the_normalized_request();
  nearest_provider_owns_even_an_empty_menu_and_null_provider_bubbles();
  child_semantics_are_preserved_and_no_synthetic_activate_is_advertised();
  secondary_request_on_a_sibling_does_not_require_existing_focus();
  an_idle_legacy_overlay_source_does_not_mask_the_accepting_parent();
}
} // namespace
int main() { return test::run("widget_context_menu", &suite); }
