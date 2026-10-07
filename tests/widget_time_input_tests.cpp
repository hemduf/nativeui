#include "test_support.hpp"
#include <nativeui/time_input.hpp>
namespace {
void suite() {
  using V = ui::TimeInput::Value;
  ui::State<V> value{std::chrono::seconds{23 * 3600 + 59 * 60 + 17}};
  int calls{};
  ui::UI tree{ui::TimeInput{"Alarm", value}.on_change([&](V v) {
    NUI_CHECK(v == value.get());
    ++calls;
  })};
  test::MockPlatform platform;
  tree.resize({300, 80});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{59 * 60 + 17} && calls == 1);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{17} && calls == 2);
  value.set(std::chrono::seconds{9 * 3600 + 30 * 60 + 17});
  tree.dispatch(test::text("5"), platform);
  tree.dispatch(test::text("9"), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{9 * 3600 + 59 * 60 + 17});
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{9 * 3600 + 59 * 60 + 17});
  value.set(std::chrono::seconds{-1});
  ui::HeadlessRenderer renderer{{300, 80}, 1};
  NUI_CHECK(renderer.render(tree) && value.get() == std::chrono::seconds{-1});
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == std::chrono::seconds{60});
}
} // namespace
int main() { return test::run("widget_time_input", &suite); }
