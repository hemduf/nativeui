#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/toggle_button.hpp>

#include <stdexcept>

namespace ui {
struct TreeTestAccess {
  static bool has_contact(Tree &tree) {
    return tree.pointer_capture_owner(17) != nullptr;
  }
};
} // namespace ui

namespace {
ui::InputEvent key_up(ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  return event;
}
void release_uses_current_source_and_balances_capture() {
  ui::State<bool> selected{false};
  ui::UI tree{ui::ToggleButton{"Bold", selected}};
  test::MockPlatform platform;
  tree.resize({120.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f),
                platform);
  NUI_CHECK(!selected.get());
  selected.set(true);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f),
                platform);
  NUI_CHECK(!selected.get());
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 130.0f, 20.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 130.0f, 20.0f),
                platform);
  NUI_CHECK(!selected.get());
}
void key_latching_throw_recovery_and_semantics() {
  ui::State<bool> selected{false};
  bool fail = true;
  auto subscription = selected.observe([&](bool) {
    if (fail)
      throw std::runtime_error("injected toggle button observer");
  });
  ui::UI tree{ui::ToggleButton{"Bold", selected}};
  test::MockPlatform platform;
  tree.resize({120.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(!selected.get());
  bool caught = false;
  try {
    tree.dispatch(key_up(ui::Key::Space), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && selected.get());
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!selected.get());
  tree.dispatch(key_up(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get());
  ui::detail::ToggleButtonComponent projection{
      "Bold", selected.binding(), {}, false};
  const auto info = projection.semantics();
  NUI_CHECK(info.role == ui::SemanticRole::Button);
  NUI_CHECK(info.name == "Bold");
  NUI_CHECK(info.checked == ui::SemanticCheckedState::Checked);
}
void selected_metrics_invalidate_layout_and_dead_source_is_inert() {
  auto selected = std::make_unique<ui::State<bool>>(false);
  ui::ToggleButtonStyle style;
  style.base.minimum_width = 0.0f;
  style.base.horizontal_padding = 0.0f;
  style.selected.minimum_width = 200.0f;
  ui::UI tree{ui::ToggleButton{"Bold", selected->binding()}.style(style)};
  const auto before = tree.measure().preferred.w;
  selected->set(true);
  NUI_CHECK(before < 200.0f);
  NUI_CHECK(tree.measure().preferred.w == 200.0f);
  selected.reset();
  test::MockPlatform platform;
  tree.resize({220.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  tree.dispatch(key_up(ui::Key::Space), platform);
  NUI_CHECK(platform.pointer_capture_begin_count == 0);
  ui::HeadlessRenderer renderer{{220.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
}
void focus_throw_releases_before_invalidation() {
  ui::State<bool> value{false};
  ui::UI tree{ui::Row{ui::ToggleButton{"Bold", value}, ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({240.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f),
                platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(240, 40));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count + 1);
  tree.set_invalidation_callback([](ui::Rect) {
    throw std::runtime_error("injected focus invalidation");
  });
  bool caught = false;
  try {
    tree.dispatch(test::key(ui::Key::Tab), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  tree.clear_invalidation_callback();
  NUI_CHECK(caught);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.paint(*surface->getCanvas(), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void dead_binding_key_releases_existing_contact() {
  auto value = std::make_unique<ui::State<bool>>(false);
  test::MockPlatform platform;
  ui::Tree tree{
      ui::compile(ui::make_spec(ui::ToggleButton{"Bold", value->binding()}))};
  tree.mount();
  tree.layout({120, 40});
  tree.activate_focus(platform);
  auto down = test::pointer(ui::InputType::PointerDown, 20, 20);
  down.pointer.id = 17;
  down.pointer.type = ui::PointerType::Touch;
  tree.dispatch(down, platform);
  auto has_capture = [&] { return ui::TreeTestAccess::has_contact(tree); };
  NUI_CHECK(has_capture());
  value.reset();
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(!has_capture());
  tree.deactivate_focus(platform);
}
void suite() {
  focus_throw_releases_before_invalidation();
  dead_binding_key_releases_existing_contact();
  release_uses_current_source_and_balances_capture();
  key_latching_throw_recovery_and_semantics();
  selected_metrics_invalidate_layout_and_dead_source_is_inert();
}
} // namespace
int main() { return test::run("widget_toggle_button", &suite); }
