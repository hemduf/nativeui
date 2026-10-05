#include "test_support.hpp"
#include <nativeui/popover.hpp>
#include <nativeui/column.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void settle(ui::UI& tree, ui::HeadlessRenderer& renderer) {
  for (int pass = 0; pass < 4; ++pass) NUI_CHECK(renderer.render(tree));
}
ui::Spec popover(ui::State<bool>& open, std::function<void()> closed) {
  return ui::make_spec(ui::Column{
      ui::Popover{open, ui::Button{"Open", [] {}}, ui::Label{"Body"}}
          .match_anchor_width(false).on_close(std::move(closed)),
      ui::Spacer{0, 100}}.padding(0).gap(0));
}
struct CopyState {
  bool armed{};
  int copies{}, calls{};
  std::function<void()> reopen;
};
struct CopyClose {
  std::shared_ptr<CopyState> state;
  explicit CopyClose(std::shared_ptr<CopyState> value) : state(std::move(value)) {}
  CopyClose(const CopyClose& other) : state(other.state) {
    if (!std::exchange(state->armed, false)) return;
    ++state->copies;
    auto reopen = state->reopen;
    if (reopen) reopen();
  }
  CopyClose(CopyClose&&) noexcept = default;
  void operator()() const { ++state->calls; }
};
void reopen_in_copy_retires_old_unstarted_notification() {
  ui::State<bool> open{false};
  auto state = std::make_shared<CopyState>();
  ui::UI tree{popover(open, CopyClose{state})};
  test::MockPlatform platform;
  tree.resize({240, 180});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{240, 180}, 1};
  settle(tree, renderer);
  open.set(true);
  settle(tree, renderer);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  state->reopen = [&] { NUI_CHECK(!open.get()); open.set(true); };
  state->armed = true;
  tree.dispatch(test::key(ui::Key::Escape), platform);
  settle(tree, renderer);
  NUI_CHECK(state->copies == 1 && !state->armed && open.get());
  NUI_CHECK(state->calls == 0 && tree.overlay_entries().size() == 1);
  state->reopen = {};
  tree.dispatch(test::key(ui::Key::Escape), platform);
  settle(tree, renderer);
  NUI_CHECK(!open.get() && state->calls == 1 && tree.overlay_entries().empty());
  tree.deactivate(platform);
}
void reopen_after_observer_throw_retires_old_unstarted_notification() {
  ui::State<bool> open{false};
  bool armed{};
  auto first = open.observe([&](bool value) {
    if (!value && std::exchange(armed, false))
      throw std::runtime_error("false observer before Popover subscription");
  });
  int calls{};
  ui::UI tree{popover(open, [&] { NUI_CHECK(!open.get()); ++calls; })};
  test::MockPlatform platform;
  tree.resize({240, 180});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{240, 180}, 1};
  settle(tree, renderer);
  open.set(true);
  settle(tree, renderer);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  armed = true;
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Escape), platform);
  } catch (const std::runtime_error& error) {
    NUI_CHECK(std::string_view{error.what()} == "false observer before Popover subscription");
    caught = true;
  }
  NUI_CHECK(caught && !armed && !open.get() && calls == 0);
  open.set(true);
  settle(tree, renderer);
  NUI_CHECK(open.get() && calls == 0 && tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  settle(tree, renderer);
  NUI_CHECK(!open.get() && calls == 1 && tree.overlay_entries().empty());
  tree.deactivate(platform);
}
void suite() {
  reopen_in_copy_retires_old_unstarted_notification();
  reopen_after_observer_throw_retires_old_unstarted_notification();
}
} // namespace
int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "reopen_in_copy") return test::run("popover_reopen_in_copy", &reopen_in_copy_retires_old_unstarted_notification);
  if (mode == "reopen_after_observer_throw") return test::run("popover_reopen_after_observer_throw", &reopen_after_observer_throw_retires_old_unstarted_notification);
  if (mode == "all") return test::run("popover_close_revision", &suite);
  return 2;
}
