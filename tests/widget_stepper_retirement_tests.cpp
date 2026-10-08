#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/stepper.hpp>

namespace {
void removal_by_invalidation_cannot_publish_retired_stepper_value() {
  ui::State<bool> present{true};
  ui::State<double> value{1};
  int notifications{};
  auto subscription = value.observe([&](double) { ++notifications; });
  ui::UI tree{ui::If{present, ui::Stepper{value}.range(0, 10)}};
  test::MockPlatform platform;
  tree.resize({100, 40});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 40));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  bool remove = true;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!remove)
      return;
    remove = false;
    present.set(false);
    tree.resize({101, 40});
  });
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 8), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!present.get() && value.get() == 1.0 && notifications == 0);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  present.set(true);
  tree.resize({100, 40});
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 8), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 8), platform);
  NUI_CHECK(value.get() == 2.0 && notifications == 1);
}
} // namespace
int main() {
  return test::run(
      "widget_stepper_retirement",
      &removal_by_invalidation_cannot_publish_retired_stepper_value);
}
