#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/spinner.hpp>
#include <nativeui/visibility.hpp>

#include <chrono>
#include <limits>
#include <memory>
#include <vector>

namespace {
using namespace std::chrono_literals;
struct Harness {
  std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
      std::make_shared<ui::detail::ManualDispatcherClock>()};
  ui::detail::DispatcherOwner owner{{}, clock};
  test::MockPlatform platform;
  Harness() { platform.dispatcher_value = owner.dispatcher(); }
  void advance(ui::DispatcherDuration duration) {
    clock->advance(duration);
    (void)owner.checkpoint();
  }
};
std::vector<SkColor> paint(ui::UI &tree, test::MockPlatform &platform) {
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(40, 40));
  NUI_CHECK(surface);
  surface->getCanvas()->clear(SK_ColorBLACK);
  tree.paint(*surface->getCanvas(), platform);
  SkPixmap pixels;
  NUI_CHECK(surface->peekPixels(&pixels));
  std::vector<SkColor> snapshot;
  for (int y = 0; y < 40; ++y)
    for (int x = 0; x < 40; ++x)
      snapshot.push_back(pixels.getColor(x, y));
  return snapshot;
}
void cycle_active_and_hidden() {
  Harness h;
  ui::State<bool> working{true};
  ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
  ui::SpinnerStyle style;
  style.color = ui::Color{1.0f, 0.0f, 0.0f, 1.0f};
  ui::UI tree{ui::Visibility{
      visibility,
      ui::Spinner{"Loading"}.active(working).size(36.0).style(style)}};
  tree.resize({40.0f, 40.0f});
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  const auto initial = paint(tree, h.platform);
  h.advance(450ms);
  const auto middle = paint(tree, h.platform);
  NUI_CHECK(initial != middle);
  h.advance(450ms);
  NUI_CHECK(paint(tree, h.platform) == initial);
  working.set(false);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  working.set(true);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  visibility.set(ui::VisibilityMode::Hidden);
  (void)paint(tree, h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  visibility.set(ui::VisibilityMode::Visible);
  (void)paint(tree, h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void reduced_zero_and_source_lifetime() {
  Harness h;
  auto source = std::make_unique<ui::State<bool>>(true);
  ui::detail::SpinnerComponent projection{
      "Task", source->binding(), {}, true, {}};
  NUI_CHECK(projection.semantics().role == ui::SemanticRole::ProgressBar);
  NUI_CHECK(!projection.semantics().numeric_value);
  ui::UI reduced{
      ui::Spinner{"Task"}.active(source->binding()).reduced_motion()};
  reduced.resize({40.0f, 40.0f});
  reduced.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  source.reset();
  NUI_CHECK(projection.semantics().role == ui::SemanticRole::None);
  (void)paint(reduced, h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  ui::UI zero{ui::Spinner{}.size(0.0)};
  zero.resize({40.0f, 40.0f});
  zero.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  NUI_CHECK(zero.measure().preferred.w == 0.0f);
}
void instances_and_validation() {
  Harness h;
  auto first = std::make_unique<ui::UI>(ui::Spinner{"First"});
  ui::UI second{ui::Spinner{"Second"}};
  first->resize({40.0f, 40.0f});
  second.resize({40.0f, 40.0f});
  first->activate(h.platform);
  second.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 2);
  first.reset();
  NUI_CHECK(h.owner.active_timer_count() == 1);
  h.advance(1s);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  second.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  bool rejected = false;
  try {
    auto invalid = ui::Spinner{}.size(std::numeric_limits<double>::quiet_NaN());
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
  ui::SpinnerStyle style;
  style.thickness_ratio = 0.0;
  rejected = false;
  try {
    auto invalid = ui::Spinner{}.style(style);
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void spinner_geometry_stops_without_paint() {
  Harness h;
  ui::UI tree{ui::Spinner{"Geometry"}};
  tree.resize({40.0f, 40.0f});
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.resize({0.0f, 0.0f});
  NUI_CHECK(h.owner.active_timer_count() == 0);
  tree.resize({40.0f, 40.0f});
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void failed_arms_are_static_and_recover() {
  Harness h;
  ui::State<bool> working{false};
  ui::UI tree{ui::Spinner{"Recovery"}.active(working).size(36.0)};
  tree.resize({40.0f, 40.0f});
  tree.activate(h.platform);
  ui::detail::DispatcherTestAccess::fail_next_timer(h.owner.dispatcher());
  working.set(true);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  const auto static_rays = paint(tree, h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  h.advance(500ms);
  NUI_CHECK(paint(tree, h.platform) == static_rays);
  working.set(false);
  working.set(true);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);

  std::vector<ui::TimerHandle> blockers;
  for (std::size_t i = 0; i < ui::kDispatcherMaxActiveTimers; ++i) {
    const auto handle = h.owner.dispatcher().schedule_after(1h, [] {});
    NUI_CHECK(handle.valid());
    blockers.push_back(handle);
  }
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == blockers.size());
  NUI_CHECK(h.owner.dispatcher().cancel(blockers.back()));
  blockers.pop_back();
  working.set(false);
  working.set(true);
  NUI_CHECK(h.owner.active_timer_count() == blockers.size() + 1);
  tree.deactivate(h.platform);
  for (const auto &handle : blockers)
    NUI_CHECK(h.owner.dispatcher().cancel(handle));
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void suite() {
  failed_arms_are_static_and_recover();
  spinner_geometry_stops_without_paint();
  cycle_active_and_hidden();
  reduced_zero_and_source_lifetime();
  instances_and_validation();
}
} // namespace
int main() { return test::run("widget_spinner", &suite); }
