#include "test_support.hpp"
#include <nativeui/if.hpp>

namespace {
struct KeyState {
  bool armed{}, remove_on_copy{}, fail{};
  int calls{};
  ui::EventResult result{ui::EventResult::Ignored};
  std::function<void()> retire;
};
struct KeyHandler {
  std::shared_ptr<KeyState> state;
  explicit KeyHandler(std::shared_ptr<KeyState> value)
      : state(std::move(value)) {}
  KeyHandler(const KeyHandler &other) : state(other.state) {
    if (state->remove_on_copy && std::exchange(state->armed, false)) {
      auto retire = state->retire;
      retire();
    }
  }
  KeyHandler(KeyHandler &&) = default;
  ui::EventResult operator()(const ui::InputEvent &) const {
    const auto owned = state;
    ++owned->calls;
    if (!owned->remove_on_copy && std::exchange(owned->armed, false)) {
      auto retire = owned->retire;
      retire();
      if (owned->fail)
        throw std::runtime_error("retired key callback");
    }
    return owned->result;
  }
};
void copy_and_invocation_retirement_suppress_old_editor_and_recover() {
  for (const bool copy : {false, true}) {
    for (const auto result :
         {ui::EventResult::Ignored, ui::EventResult::Handled}) {
      for (const bool fail : {false, true}) {
        if (copy && fail)
          continue;
        auto state = std::make_shared<KeyState>();
        state->remove_on_copy = copy;
        state->result = result;
        state->fail = fail;
        ui::State<std::string> value{"abc"};
        ui::State<bool> present{true};
        test::MockPlatform platform;
        ui::UI tree{ui::If{present, ui::TextInput{"Name", value}.on_key_down(
                                        KeyHandler{state})}};
        tree.resize({200, 80});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{200, 80}, 1};
        NUI_CHECK(renderer.render(tree));
        state->retire = [&] {
          present.set(false);
          tree.resize({201, 80});
        };
        state->armed = true;
        bool caught{};
        try {
          tree.dispatch(test::key(ui::Key::Backspace), platform);
        } catch (const std::runtime_error &error) {
          NUI_CHECK(std::string_view{error.what()} == "retired key callback");
          caught = true;
        }
        NUI_CHECK(caught == fail && !state->armed && !present.get());
        NUI_CHECK(state->calls == (copy ? 0 : 1));
        NUI_CHECK(value.get() == "abc" && !platform.text_input_active);
        state->retire = {};
        state->fail = false;
        present.set(true);
        tree.resize({200, 80});
        NUI_CHECK(renderer.render(tree));
        // A pointer contact focuses the newly mounted editor before the next
        // key command. Its key hook and normal text publication remain usable.
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 35),
                      platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 35),
                      platform);
        const auto previous = state->calls;
        tree.dispatch(test::key(ui::Key::End), platform);
        NUI_CHECK(state->calls == previous + 1);
        tree.dispatch(test::text("Z"), platform);
        NUI_CHECK(value.get().find('Z') != std::string::npos);
        tree.deactivate(platform);
        NUI_CHECK(!platform.text_input_active);
        NUI_CHECK(platform.pointer_capture_begin_count ==
                  platform.pointer_capture_end_count);
      }
    }
  }
}
} // namespace
int main() {
  return test::run(
      "text_input_key_retirement",
      copy_and_invocation_retirement_suppress_old_editor_and_recover);
}
