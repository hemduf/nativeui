#include "test_support.hpp"

#include <nativeui/radio_button.hpp>

#include <string_view>
#include <utility>

namespace {

// Real RadioGroup/RadioButton + retained Tree routing. The injected callback is
// the production UI exposure callback, entered before the old arrow selection.
// Mount guards stay usable after a legitimate reactivation; the Core must pin
// the originating input activation separately before invoking the participant.
void previous_arrow_does_not_select_after_reactivation() {
  ui::State<int> selected{1};
  int notifications{};
  auto subscription = selected.observe([&](int) { ++notifications; });
  ui::RadioGroup<int> group{selected};
  ui::UI tree{ui::Column{ui::RadioButton{group, 1, "One"},
                       ui::RadioButton{group, 2, "Two"}}};
  test::MockPlatform platform;
  tree.resize({200.0f, 100.0f});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{200.0f, 100.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));

  bool armed{};
  bool value_was_unpublished{};
  int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    ++boundaries;
    value_was_unpublished = selected.get() == 1 && notifications == 0;
    // Deactivation terminalizes the previous focus transition. Reactivation is
    // a new input owner and restores the selected first radio's focus.
    tree.deactivate(platform);
    tree.activate(platform);
  });
  armed = true;
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(boundaries == 1);
  NUI_CHECK(value_was_unpublished);
  NUI_CHECK(selected.get() == 1);
  NUI_CHECK(notifications == 0);

  tree.clear_invalidation_callback();
  // A new native input belongs to the new activation and must still work.
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(selected.get() == 2);
  NUI_CHECK(notifications == 1);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(selected.get() == 1);
  NUI_CHECK(notifications == 2);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}

} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "epoch" || mode == "all")
    return test::run("roving_focus_activation_epoch",
                     &previous_arrow_does_not_select_after_reactivation);
  return 2;
}
