#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/progress_bar.hpp>
#include <nativeui/visibility.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace {
using namespace std::chrono_literals;
ui::ProgressBarStyle style() {
  ui::ProgressBarStyle value;
  value.base.track = ui::Color{0.0f, 0.0f, 0.0f, 1.0f};
  value.base.fill = ui::Color{1.0f, 0.0f, 0.0f, 1.0f};
  value.base.border_width = 0.0f;
  value.base.corner_radius = 0.0f;
  value.base.fill_corner_radius = 0.0f;
  return value;
}
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
bool red(ui::HeadlessRenderer &renderer, int x) {
  const auto pixel = renderer.pixel(x, 5);
  return pixel.r > 240 && pixel.g < 5 && pixel.b < 5;
}
void static_segment_and_semantics() {
  ui::State<float> value{0.8f};
  int formatter_calls{};
  ui::UI tree{ui::ProgressBar{value}.indeterminate().style(style()).formatter(
      [&](float) {
        ++formatter_calls;
        return "unexpected";
      })};
  ui::HeadlessRenderer renderer{{100.0f, 10.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(red(renderer, 50) && !red(renderer, 20) && !red(renderer, 80));
  NUI_CHECK(formatter_calls == 0);
  ui::detail::BoundedDisplayComponent component{
      value.binding(), 0.0f,  1.0f, ui::ProgressOrientation::Horizontal, {},
      style(),         false, true};
  const auto info = component.semantics();
  NUI_CHECK(info.role == ui::SemanticRole::ProgressBar);
  NUI_CHECK(!info.numeric_value && !info.value_range);
  NUI_CHECK(info.description == "indeterminate progress");
}
void clock_cycle_and_hidden_stop() {
  Harness h;
  ui::State<float> value{0.8f};
  ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
  ui::UI tree{ui::Visibility{
      visibility, ui::ProgressBar{value}.indeterminate().style(style())}};
  tree.resize({100.0f, 10.0f});
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 10));
  NUI_CHECK(surface);
  h.advance(700ms);
  tree.paint(*surface->getCanvas(), h.platform);
  SkPixmap pixels;
  NUI_CHECK(surface->peekPixels(&pixels));
  NUI_CHECK(SkColorGetR(pixels.getColor(50, 5)) > 240);
  NUI_CHECK(SkColorGetR(pixels.getColor(10, 5)) == 0);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  h.advance(700ms);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  visibility.set(ui::VisibilityMode::Hidden);
  tree.paint(*surface->getCanvas(), h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  visibility.set(ui::VisibilityMode::Visible);
  tree.paint(*surface->getCanvas(), h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void reduced_motion_lifetime_and_isolation() {
  Harness h;
  auto source = std::make_unique<ui::State<float>>(0.8f);
  auto moving = std::make_unique<ui::UI>(
      ui::ProgressBar{source->binding()}.indeterminate());
  moving->resize({100.0f, 10.0f});
  moving->activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  ui::State<float> other{0.2f};
  ui::UI reduced{
      ui::ProgressBar{other}.indeterminate().reduced_motion().style(style())};
  reduced.resize({100.0f, 10.0f});
  reduced.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  ui::HeadlessRenderer renderer{{100.0f, 10.0f}, 1.0f};
  NUI_CHECK(renderer.render(reduced));
  NUI_CHECK(red(renderer, 50));
  source.reset();
  h.advance(20ms);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  moving.reset();
  h.advance(1s);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  NUI_CHECK(renderer.render(reduced));
  NUI_CHECK(red(renderer, 50));
}
void progress_geometry_stops_without_paint() {
  Harness h;
  ui::State<float> value{0.4f};
  ui::UI tree{ui::ProgressBar{value}.indeterminate()};
  tree.resize({100.0f, 10.0f});
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.resize({0.0f, 0.0f});
  NUI_CHECK(h.owner.active_timer_count() == 0);
  tree.resize({100.0f, 10.0f});
  NUI_CHECK(h.owner.active_timer_count() == 1);
  tree.deactivate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void failed_arms_are_static_and_recover() {
  Harness h;
  ui::State<float> value{0.4f};
  ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
  ui::UI tree{ui::Visibility{
      visibility, ui::ProgressBar{value}.indeterminate().style(style())}};
  tree.resize({100.0f, 10.0f});
  ui::detail::DispatcherTestAccess::fail_next_timer(h.owner.dispatcher());
  tree.activate(h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 10));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  SkPixmap pixels;
  NUI_CHECK(surface->peekPixels(&pixels));
  NUI_CHECK(SkColorGetR(pixels.getColor(50, 5)) > 240);
  visibility.set(ui::VisibilityMode::Hidden);
  tree.resize({100.0f, 10.0f});
  NUI_CHECK(h.owner.active_timer_count() == 0);
  visibility.set(ui::VisibilityMode::Visible);
  tree.resize({100.0f, 10.0f});
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
  visibility.set(ui::VisibilityMode::Hidden);
  tree.resize({100.0f, 10.0f});
  visibility.set(ui::VisibilityMode::Visible);
  tree.resize({100.0f, 10.0f});
  NUI_CHECK(h.owner.active_timer_count() == blockers.size() + 1);
  tree.deactivate(h.platform);
  for (const auto &handle : blockers)
    NUI_CHECK(h.owner.dispatcher().cancel(handle));
  NUI_CHECK(h.owner.active_timer_count() == 0);
}
void suite() {
  failed_arms_are_static_and_recover();
  progress_geometry_stops_without_paint();
  static_segment_and_semantics();
  clock_cycle_and_hidden_stop();
  reduced_motion_lifetime_and_isolation();
}
} // namespace
int main() { return test::run("widget_progress_activity", &suite); }
