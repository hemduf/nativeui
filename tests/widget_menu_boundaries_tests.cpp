#include "../src/detail/widget_menu_popup.hpp"
#include "test_support.hpp"
#include <nativeui/combo_popup.hpp>
namespace {
void noop() {}
ui::InputContext context(test::MockPlatform &platform) {
  return {{0, 0, 140, 40}, platform, noop, noop, noop, noop};
}
ui::MountContext mount_context(ui::NodeId id) { return {id, noop, noop, noop}; }
ui::LifecycleContext lifecycle(ui::NodeId id) {
  return {id, {0, 0, 140, 40}, noop, noop};
}
void release(ui::Component &component, ui::InputContext &ctx, ui::Key key) {
  auto up = test::key(key);
  up.type = ui::InputType::KeyUp;
  component.input(up, ctx);
}
void callback_started_before_an_exception_is_one_shot() {
  auto anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
  anchor->mounted = true;
  anchor->node_id = 23;
  auto session = std::make_shared<ui::detail::MenuPopupSession>();
  session->anchor = anchor;
  int calls{};
  session->items = {ui::PopupMenuItem::action("Action", [&] {
    ++calls;
    throw std::runtime_error("action fault");
  })};
  session->highlighted = 0;
  ui::detail::MenuPopupComponent component{session};
  test::MockPlatform platform;
  auto ctx = context(platform);
  component.input(test::key(ui::Key::Enter), ctx);
  auto command = component.take_overlay_command();
  NUI_CHECK(command && command->after_close);
  bool caught{};
  try {
    command->after_close();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1);
  try {
    command->after_close();
  } catch (const std::runtime_error &) {
  }
  NUI_CHECK(calls == 1);
}
ui::OverlayHandle handle(ui::UI &owner) {
  ui::OverlaySpec overlay;
  overlay.content = ui::Label{"Independent overlay"}.spec();
  return owner.show_overlay(std::move(overlay));
}
void old_on_shown_cannot_attach_to_a_deactivated_anchor() {
  auto anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
  ui::detail::PopupMenuComponent component{
      "Actions", [] { return std::vector<ui::PopupMenuItem>{}; },
      ui::ComboBoxStyle{}, ui::MenuItemStyle{}, anchor};
  auto mount = mount_context(23);
  auto life = lifecycle(23);
  component.mount(mount);
  component.activate(life);
  test::MockPlatform platform;
  auto ctx = context(platform);
  component.input(test::key(ui::Key::Down), ctx);
  auto old = component.take_overlay_command();
  NUI_CHECK(old && old->on_shown);
  component.deactivate(life);
  ui::UI owner{ui::Label{"Owner"}};
  owner.resize({300, 200});
  auto live_handle = handle(owner);
  NUI_CHECK(live_handle.valid());
  old->on_shown(live_handle);
  NUI_CHECK(!anchor->handle.valid());
  component.unmount(life);
}
void a_completed_old_action_cannot_run_after_a_new_open() {
  auto anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
  int calls{}, providers{};
  ui::detail::PopupMenuComponent component{
      "Actions",
      [&] {
        ++providers;
        return std::vector<ui::PopupMenuItem>{
            ui::PopupMenuItem::action("Action", [&] { ++calls; })};
      },
      ui::ComboBoxStyle{}, ui::MenuItemStyle{}, anchor};
  auto mount = mount_context(23);
  auto life = lifecycle(23);
  component.mount(mount);
  component.activate(life);
  test::MockPlatform platform;
  auto ctx = context(platform);
  component.input(test::key(ui::Key::Down), ctx);
  auto show = component.take_overlay_command();
  NUI_CHECK(show && show->on_shown);
  ui::UI owner{ui::Label{"Owner"}};
  owner.resize({300, 200});
  auto old_handle = handle(owner);
  show->on_shown(old_handle);
  auto panel = show->overlay.content.factory();
  release(*panel, ctx, ui::Key::Down);
  panel->input(test::key(ui::Key::Enter), ctx);
  auto *source = dynamic_cast<ui::detail::OverlayCommandSource *>(panel.get());
  NUI_CHECK(source);
  auto action = source->take_overlay_command();
  NUI_CHECK(action && action->after_close);
  owner.close_overlay(old_handle);
  release(component, ctx, ui::Key::Enter);
  component.input(test::key(ui::Key::Down), ctx);
  auto fresh = component.take_overlay_command();
  NUI_CHECK(fresh && providers == 2);
  action->after_close();
  NUI_CHECK(calls == 0);
  component.unmount(life);
}
void queued_action_is_inert_after_unmount() {
  auto anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
  anchor->mounted = true;
  anchor->node_id = 23;
  auto session = std::make_shared<ui::detail::MenuPopupSession>();
  session->anchor = anchor;
  int calls{};
  session->items = {ui::PopupMenuItem::action("Action", [&] { ++calls; })};
  session->highlighted = 0;
  ui::detail::MenuPopupComponent component{session};
  test::MockPlatform platform;
  auto ctx = context(platform);
  component.input(test::key(ui::Key::Enter), ctx);
  auto command = component.take_overlay_command();
  NUI_CHECK(command && command->after_close);
  anchor->mounted = false;
  command->after_close();
  NUI_CHECK(calls == 0);
}
void queued_submenu_completion_does_not_retain_a_retired_session() {
  auto anchor = std::make_shared<ui::detail::MenuAnchorRuntime>();
  anchor->mounted = true;
  anchor->node_id = 23;
  std::weak_ptr<ui::detail::MenuPopupSession> root_weak, child_weak;
  {
    auto root = std::make_shared<ui::detail::MenuPopupSession>();
    root->anchor = anchor;
    auto parent = std::make_unique<ui::detail::MenuPopupComponent>(root);
    auto child = std::make_shared<ui::detail::MenuPopupSession>();
    child->anchor = anchor;
    child->root = root;
    child->parent = root;
    child->items = {ui::PopupMenuItem::action("Action", [] {})};
    child->highlighted = 0;
    root->child = child;
    ui::detail::MenuPopupComponent panel{child};
    test::MockPlatform platform;
    auto ctx = context(platform);
    panel.input(test::key(ui::Key::Enter), ctx);
    NUI_CHECK(panel.has_pending_overlay_command());
    root_weak = root;
    child_weak = child;
  }
  ui::detail::retire_menu_anchor(anchor);
  NUI_CHECK(root_weak.expired() && child_weak.expired());
}
void suite() {
  callback_started_before_an_exception_is_one_shot();
  old_on_shown_cannot_attach_to_a_deactivated_anchor();
  a_completed_old_action_cannot_run_after_a_new_open();
  queued_action_is_inert_after_unmount();
  queued_submenu_completion_does_not_retain_a_retired_session();
}
} // namespace
int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "all";
  if (mode == "once")
    return test::run(mode.c_str(),
                     &callback_started_before_an_exception_is_one_shot);
  if (mode == "deactivate")
    return test::run(mode.c_str(),
                     &old_on_shown_cannot_attach_to_a_deactivated_anchor);
  if (mode == "generation")
    return test::run(mode.c_str(),
                     &a_completed_old_action_cannot_run_after_a_new_open);
  if (mode == "unmount")
    return test::run(mode.c_str(), &queued_action_is_inert_after_unmount);
  if (mode == "ownership")
    return test::run(
        mode.c_str(),
        &queued_submenu_completion_does_not_retain_a_retired_session);
  return test::run("widget_menu_boundaries", &suite);
}
