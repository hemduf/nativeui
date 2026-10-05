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
  test::MockPlatform platform;
  ui::UI tree{ui::If{present,
                     ui::RichText{std::vector<ui::RichTextSpan>{
                         {.id = "action", .text = "Action", .on_activate = [&] {
                            ++calls;
                          }}}}}};
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
  for (const std::string_view activation :
       {"keyboard", "pointer", "semantic"}) {
    auto state = std::make_shared<CopyState>();
    ui::State<bool> present{true};
    test::MockPlatform platform;
    ui::Component *action{};
    auto spec = ui::RichText{std::vector<ui::RichTextSpan>{
                                 {.id = "action",
                                  .text = "Action",
                                  .on_activate = CopyAction{state}}}}
                    .spec();
    auto children = spec.children_factory;
    spec.children_factory = [&action, children](ui::Component &component) {
      auto result = children(component);
      NUI_CHECK(result.size() == 1);
      auto create = result.front().factory;
      result.front().factory = [&action, create] {
        auto value = create();
        action = value.get();
        return value;
      };
      return result;
    };
    ui::UI tree{ui::If{present, std::move(spec)}};
    tree.resize({120, 70});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{120, 70}, 1};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(action);
    if (activation == "pointer")
      tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10),
                    platform);
    state->retire = [&] {
      present.set(false);
      tree.resize({121, 70});
    };
    state->armed = true;
    if (activation == "keyboard")
      tree.dispatch(test::key(ui::Key::Enter), platform);
    else if (activation == "pointer")
      tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
    else {
      // Direct component semantic delivery uses a callback-duration borrow,
      // without claiming a native accessibility bridge is available.
      ui::InputContext context{
          {0, 0, 120, 70}, platform, [&] { NUI_CHECK(present.get()); },
          [] {},           [] {},    [] {}};
      NUI_CHECK(action->semantic_action(ui::SemanticAction::Activate,
                                        context) == ui::EventResult::Handled);
    }
    NUI_CHECK(!state->armed && !present.get() && state->calls == 0);
    state->retire = {};
    present.set(true);
    NUI_CHECK(renderer.render(tree));
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
    NUI_CHECK(state->calls == 1);
    tree.deactivate(platform);
    NUI_CHECK(platform.pointer_capture_begin_count ==
              platform.pointer_capture_end_count);
  }
}
void focus_request_removal_retires_pointer_before_borrowed_invalidation() {
  ui::State<bool> present{true};
  int calls{};
  test::MockPlatform platform;
  ui::UI tree{ui::If{present,
                     ui::RichText{std::vector<ui::RichTextSpan>{
                         {.id = "action", .text = "Action", .on_activate = [&] {
                            ++calls;
                          }}}}}};
  tree.resize({120, 70});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{120, 70}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false))
      return;
    present.set(false);
    tree.resize({121, 70});
  });
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && !present.get() && calls == 0);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  present.set(true);
  NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
  NUI_CHECK(calls == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void suite() {
  reflow_prepare_failures_leave_the_committed_layout_and_recover();
  invalidation_removal_suppresses_prepared_action_and_capture_recovers();
  callback_copy_retires_the_action_before_invocation();
  focus_request_removal_retires_pointer_before_borrowed_invalidation();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "callback_copy")
    return test::run("rich_text_callback_copy",
                     callback_copy_retires_the_action_before_invocation);
  if (argc == 2 && std::string_view{argv[1]} == "focus_removal")
    return test::run(
        "rich_text_focus_removal",
        focus_request_removal_retires_pointer_before_borrowed_invalidation);
  return test::run("rich_text_recovery", suite);
}
