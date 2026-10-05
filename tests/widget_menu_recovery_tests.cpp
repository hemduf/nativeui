#include "test_support.hpp"
#include <nativeui/context_menu.hpp>
#include <nativeui/popup_menu.hpp>
namespace {
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void action_is_closed_before_callback_and_a_throw_is_never_replayed() {
  bool fail = true;
  int calls{};
  ui::UI *owner{};
  ui::UI tree{ui::PopupMenu{
      "Actions",
      std::vector<ui::PopupMenuItem>{ui::PopupMenuItem::action("Action", [&] {
        ++calls;
        NUI_CHECK(owner && owner->overlay_entries().empty());
        if (fail)
          throw std::runtime_error("application action fault");
      })}}};
  owner = &tree;
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1 && tree.overlay_entries().empty());
  ui::HeadlessRenderer renderer{{400, 300}, 1};
  NUI_CHECK(renderer.render(tree) && calls == 1);
  release(tree, platform, ui::Key::Enter);
  fail = false;
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 2 && tree.overlay_entries().empty());
}
void provider_throw_and_reentrant_disable_leave_a_recoverable_closed_anchor() {
  bool fail = true, disable{};
  int providers{}, calls{};
  ui::State<bool> enabled{true};
  ui::UI tree{ui::Enabled{
      enabled, ui::PopupMenu{"Actions", [&] {
                               ++providers;
                               if (fail)
                                 throw std::runtime_error("provider fault");
                               if (disable)
                                 enabled.set(false);
                               return std::vector<ui::PopupMenuItem>{
                                   ui::PopupMenuItem::action("Action",
                                                             [&] { ++calls; })};
                             }}}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Down), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && providers == 1 && calls == 0 &&
            tree.overlay_entries().empty());
  release(tree, platform, ui::Key::Down);
  fail = false;
  disable = true;
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(providers == 2 && calls == 0 && tree.overlay_entries().empty());
  disable = false;
  enabled.set(true);
  tree.dispatch(test::key(ui::Key::Down), platform);
  release(tree, platform, ui::Key::Down);
  NUI_CHECK(providers == 3 && tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1 && tree.overlay_entries().empty());
}
void context_provider_failure_preserves_the_primary_child() {
  bool fail = true;
  int clicks{}, actions{};
  auto content = ui::Button{"Content", [&] { ++clicks; }}.spec();
  ui::UI tree{ui::ContextMenu{
      std::move(content), [&] {
        if (fail)
          throw std::runtime_error("context provider fault");
        return std::vector<ui::PopupMenuItem>{
            ui::PopupMenuItem::action("Action", [&] { ++actions; })};
      }}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::pointer(ui::InputType::ContextMenu, 10, 10), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && tree.overlay_entries().empty() && clicks == 0);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
  NUI_CHECK(clicks == 1 && actions == 0);
  fail = false;
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 10, 10), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(actions == 1 && clicks == 1 && tree.overlay_entries().empty());
}
struct ProviderCopyState {
  bool armed{};
  int calls{};
  std::function<void()> on_copy;
};
struct ProviderCopy {
  std::shared_ptr<ProviderCopyState> state;
  explicit ProviderCopy(std::shared_ptr<ProviderCopyState> value)
      : state(std::move(value)) {}
  ProviderCopy(const ProviderCopy &other) : state(other.state) {
    if (state->armed) {
      auto callback = std::exchange(state->on_copy, {});
      if (callback)
        callback();
    }
  }
  std::vector<ui::PopupMenuItem> operator()() const {
    ++state->calls;
    return {};
  }
};
void provider_copy_revalidates_before_the_provider_begins() {
  auto copy = std::make_shared<ProviderCopyState>();
  ui::State<bool> enabled{true};
  ui::UI tree{ui::Enabled{enabled,
                          ui::PopupMenu{"Actions", ui::PopupMenu::ItemsProvider{
                                                       ProviderCopy{copy}}}}};
  test::MockPlatform platform;
  tree.resize({400, 300});
  tree.activate(platform);
  copy->on_copy = [&] { enabled.set(false); };
  copy->armed = true;
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(!enabled.get() && copy->calls == 0 &&
            tree.overlay_entries().empty());
  copy->armed = false;
  enabled.set(true);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(copy->calls == 1 && tree.overlay_entries().size() == 1);
}
void suite() {
  action_is_closed_before_callback_and_a_throw_is_never_replayed();
  provider_throw_and_reentrant_disable_leave_a_recoverable_closed_anchor();
  context_provider_failure_preserves_the_primary_child();
  provider_copy_revalidates_before_the_provider_begins();
}
} // namespace
int main() { return test::run("widget_menu_recovery", &suite); }
