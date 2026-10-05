#include "test_support.hpp"
#include <nativeui/date_input.hpp>
namespace {
void suite() {
  using V = ui::DateInput::Value;
  const auto initial = std::chrono::sys_days{std::chrono::year{2026} / 10 / 4};
  ui::State<V> value{initial};
  int calls{};
  ui::UI tree{ui::DateInput{"Day", value}.on_change([&](V v) {
    NUI_CHECK(v == value.get());
    ++calls;
  })};
  test::MockPlatform platform;
  tree.resize({500, 450});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tree.overlay_entries().size() == 1 && calls == 0);
  ui::HeadlessRenderer renderer{{500, 450}};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.structural_diagnostic().empty());
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(tree.overlay_entries().empty() && value.get() == initial);
  ui::InputEvent up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(tree.overlay_entries().empty() && calls == 1 &&
            value.get() == initial + std::chrono::days{1});
}
} // namespace
int main() { return test::run("widget_date_input", &suite); }
