#include "test_support.hpp"
#include <nativeui/color_well.hpp>
namespace {
void suite() {
  ui::State<ui::Color> color{ui::Color{1, 0, 0, .25f}};
  int calls{};
  ui::UI tree{ui::ColorWell{"Color", color}.on_change([&](ui::Color c) {
    NUI_CHECK(c == color.get());
    ++calls;
  })};
  test::MockPlatform platform;
  tree.resize({500, 500});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(tree.overlay_entries().empty());
  auto release = test::key(ui::Key::Space);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(calls == 1 && color.get().a == .25f);
  const auto accepted = color.get();
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(tree.overlay_entries().empty() && color.get() == accepted &&
            calls == 1);
}
} // namespace
int main() { return test::run("widget_color_well", &suite); }
