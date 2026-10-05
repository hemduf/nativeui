#include "detail/widget_rich_text.hpp"
#include "test_support.hpp"
#include <nativeui/if.hpp>
#include <nativeui/rich_text.hpp>
#include <stdexcept>
namespace {
void reflow_prepare_failures_leave_the_committed_layout_and_recover() {
  ui::Component *component{};
  auto spec =
      ui::RichText{
          std::vector<ui::RichTextSpan>{
              {.text = "First office العربية paragraph long enough to wrap"}}}
          .spec();
  auto create = spec.factory;
  spec.factory = [&component, create] {
    auto value = create();
    component = value.get();
    return value;
  };
  ui::UI tree{spec};
  tree.resize({230, 200});
  ui::HeadlessRenderer renderer{{230, 200}, 1};
  NUI_CHECK(renderer.render(tree));
  for (const auto phase : {ui::detail::RichTextReflowPhase::FontSnapshot,
                           ui::detail::RichTextReflowPhase::Shape,
                           ui::detail::RichTextReflowPhase::Publish}) {
    const auto previous = ui::detail::RichTextTestAccess::layout(*component);
    NUI_CHECK(previous);
    ui::detail::RichTextTestAccess::fail_next_reflow(*component, phase);
    bool caught{};
    try {
      (void)component->measure_constrained(ui::Constraints::loose({71, 500}),
                                           {});
    } catch (const std::runtime_error &) {
      caught = true;
    }
    NUI_CHECK(caught &&
              ui::detail::RichTextTestAccess::layout(*component) == previous);
    const auto next =
        component->measure_constrained(ui::Constraints::loose({72, 500}), {});
    NUI_CHECK(next.preferred.h > previous->preferred.h);
    tree.resize({72, 200});
    NUI_CHECK(renderer.render(tree));
    tree.resize({230, 200});
    NUI_CHECK(renderer.render(tree));
  }
}
void invalidation_removal_suppresses_prepared_action_and_capture_recovers() {
  ui::State<bool> present{true};
  int calls{};
  ui::UI tree{ui::If{present,
                     ui::RichText{std::vector<ui::RichTextSpan>{
                         {.id = "action", .text = "Action", .on_activate = [&] {
                            ++calls;
                          }}}}}};
  test::MockPlatform platform;
  tree.resize({120, 70});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{120, 70}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (armed) {
      armed = false;
      present.set(false);
      tree.resize({121, 70});
    }
  });
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && calls == 0 && !present.get());
  present.set(true);
  NUI_CHECK(renderer.render(tree));
  tree.set_invalidation_callback(
      [](ui::Rect) { throw std::runtime_error("paint invalidation"); });
  bool caught{};
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  tree.clear_invalidation_callback();
  NUI_CHECK(caught && platform.pointer_capture_begin_count ==
                          platform.pointer_capture_end_count);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
  NUI_CHECK(calls == 1);
}
struct CopyState {
  bool armed{};
  int calls{};
  std::function<void()> retire;
};
struct CopyAction {
  std::shared_ptr<CopyState> state;
  explicit CopyAction(std::shared_ptr<CopyState> value)
      : state(std::move(value)) {}
  CopyAction(const CopyAction &other) : state(other.state) {
    if (state->armed) {
      state->armed = false;
      auto retire = state->retire;
      if (retire)
        retire();
    }
  }
  CopyAction(CopyAction &&) = default;
  void operator()() const { ++state->calls; }
};
void callback_copy_retires_the_action_before_invocation() {
  auto state = std::make_shared<CopyState>();
  ui::State<bool> present{true};
  ui::UI tree{ui::If{present, ui::RichText{std::vector<ui::RichTextSpan>{
                                  {.id = "action",
                                   .text = "Action",
                                   .on_activate = CopyAction{state}}}}}};
  test::MockPlatform platform;
  tree.resize({120, 70});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{120, 70}, 1};
  NUI_CHECK(renderer.render(tree));
  state->retire = [&] {
    present.set(false);
    tree.resize({121, 70});
  };
  state->armed = true;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!state->armed && !present.get() && state->calls == 0);
  state->retire = {};
}
void suite() {
  reflow_prepare_failures_leave_the_committed_layout_and_recover();
  invalidation_removal_suppresses_prepared_action_and_capture_recovers();
  callback_copy_retires_the_action_before_invocation();
}
} // namespace
int main() { return test::run("rich_text_recovery", suite); }
