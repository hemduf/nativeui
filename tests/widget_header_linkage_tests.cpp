#include "test_support.hpp"

ui::Spec widget_headers_forward();
ui::Spec widget_headers_reverse();

int main() {
  ui::UI first{widget_headers_forward()};
  ui::UI second{widget_headers_reverse()};
  test::MockPlatform platform_first;
  test::MockPlatform platform_second;
  first.resize({720.0f, 240.0f});
  second.resize({320.0f, 180.0f});
  first.activate(platform_first);
  second.activate(platform_second);
  ui::HeadlessRenderer renderer_first{{720.0f, 240.0f}, 1.0f};
  ui::HeadlessRenderer renderer_second{{320.0f, 180.0f}, 2.0f};
  NUI_CHECK(renderer_first.render(first));
  NUI_CHECK(renderer_second.render(second));
  first.deactivate(platform_first);
  NUI_CHECK(!second.layout_dirty());
  NUI_CHECK(!second.paint_dirty());
  second.deactivate(platform_second);
  return 0;
}
