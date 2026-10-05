#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"

namespace {
void blur_releases_selection_capture_before_throwing_invalidation() {
  ui::State<std::string> value{"selectable text"};
  ui::UI tree{ui::Row{ui::TextInput{"Text", value}, ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({600, 90});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 35, 40), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count + 1);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(600, 90));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  tree.set_invalidation_callback([](ui::Rect) {
    throw std::runtime_error("injected editor blur invalidation");
  });
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Tab), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  tree.clear_invalidation_callback();
  NUI_CHECK(caught);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.paint(*surface->getCanvas(), platform);
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 35, 40), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 65, 40), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
} // namespace
int main() {
  return test::run(
      "widget_text_input_capture",
      &blur_releases_selection_capture_before_throwing_invalidation);
}
