#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/stepper.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui {
struct TreeTestAccess {
  static bool has_contact(Tree &tree) {
    return tree.pointer_capture_owner(17) != nullptr;
  }
};
} // namespace ui
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
void repeat_respects_current_source_and_bound() {
  Harness h;
  ui::State<double> value{1.0};
  ui::UI tree{ui::Stepper{value}.label("Copies").range(0.0, 10.0)};
  tree.resize({24.0f, 40.0f});
  tree.activate(h.platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 12.0f, 8.0f),
                h.platform);
  NUI_CHECK(value.get() == 2.0);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  h.advance(399ms);
  NUI_CHECK(value.get() == 2.0);
  value.set(7.0);
  h.advance(1ms);
  NUI_CHECK(value.get() == 8.0);
  h.advance(80ms);
  NUI_CHECK(value.get() == 9.0);
  h.advance(80ms);
  NUI_CHECK(value.get() == 10.0);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 12.0f, 8.0f),
                h.platform);
  NUI_CHECK(h.platform.pointer_capture_begin_count ==
            h.platform.pointer_capture_end_count);
}
void leave_reentry_and_observer_throw_recover() {
  Harness h;
  ui::State<double> value{1.0};
  bool fail = false;
  auto subscription = value.observe([&](double) {
    if (fail)
      throw std::runtime_error("injected stepper observer");
  });
  ui::UI tree{ui::Stepper{value}.range(0.0, 20.0)};
  tree.resize({24.0f, 40.0f});
  tree.activate(h.platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 12.0f, 8.0f),
                h.platform);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 30.0f, 8.0f),
                h.platform);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  h.advance(500ms);
  NUI_CHECK(value.get() == 2.0);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 12.0f, 8.0f),
                h.platform);
  NUI_CHECK(value.get() == 2.0 && h.owner.active_timer_count() == 1);
  fail = true;
  bool caught = false;
  try {
    h.advance(400ms);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == 3.0);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  NUI_CHECK(h.platform.pointer_capture_begin_count ==
            h.platform.pointer_capture_end_count);
  fail = false;
  tree.dispatch(test::key(ui::Key::End), h.platform);
  NUI_CHECK(value.get() == 20.0);
  tree.dispatch(test::key(ui::Key::Home), h.platform);
  NUI_CHECK(value.get() == 0.0);
}
void invalid_domains_are_rejected_and_constant_is_inert() {
  ui::State<double> value{std::numeric_limits<double>::quiet_NaN()};
  bool rejected = false;
  try {
    auto invalid = ui::Stepper{value}.step(0.0);
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
  ui::UI tree{ui::Stepper{value}.range(2.0, 2.0)};
  test::MockPlatform platform;
  tree.resize({24.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(std::isnan(value.get()));
  ui::HeadlessRenderer renderer{{24.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
}
void hover_leaves_to_sibling_restores_style_and_layout() {
  ui::State<double> value{1.0};
  ui::StepperStyle style;
  style.base.width = 24.0;
  style.base.height = 40.0;
  style.base.fill = ui::Color{0, 0, 0, 1};
  style.hovered.width = 48.0;
  style.hovered.fill = ui::Color{1, 0, 0, 1};
  ui::UI tree{ui::Row{ui::Stepper{value}.style(style), ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({200, 40});
  tree.activate(platform);
  const float normal_width = tree.measure().preferred.w;
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  NUI_CHECK(surface);
  auto red_at_top_cell = [&] {
    tree.paint(*surface->getCanvas(), platform);
    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    return SkColorGetR(pixels.getColor(4, 8));
  };
  NUI_CHECK(red_at_top_cell() < 10);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 6, 8), platform);
  NUI_CHECK_NEAR(tree.measure().preferred.w, normal_width + 24.0f, 0.01f);
  NUI_CHECK(red_at_top_cell() > 240);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 80, 8), platform);
  NUI_CHECK_NEAR(tree.measure().preferred.w, normal_width, 0.01f);
  NUI_CHECK(red_at_top_cell() < 10);
}
void expired_source_repeat_releases_tracked_contact() {
  Harness h;
  auto value = std::make_unique<ui::State<double>>(1.0);
  int notifications{};
  auto subscription = value->observe([&](double) { ++notifications; });
  ui::Tree tree{ui::compile(ui::make_spec(ui::Stepper{value->binding()}))};
  tree.mount();
  tree.layout({24, 40});
  tree.activate_focus(h.platform);
  auto down = test::pointer(ui::InputType::PointerDown, 12, 8);
  down.pointer.id = 17;
  down.pointer.type = ui::PointerType::Touch;
  tree.dispatch(down, h.platform);
  NUI_CHECK(ui::TreeTestAccess::has_contact(tree));
  NUI_CHECK(value->get() == 2.0 && notifications == 1);
  NUI_CHECK(h.owner.active_timer_count() == 1);
  value.reset();
  h.advance(400ms);
  NUI_CHECK(h.owner.active_timer_count() == 0);
  NUI_CHECK(notifications == 1);
  NUI_CHECK(!ui::TreeTestAccess::has_contact(tree));
  tree.deactivate_focus(h.platform);
}
void suite() {
  expired_source_repeat_releases_tracked_contact();
  hover_leaves_to_sibling_restores_style_and_layout();
  repeat_respects_current_source_and_bound();
  leave_reentry_and_observer_throw_recover();
  invalid_domains_are_rejected_and_constant_is_inert();
}
} // namespace
int main() { return test::run("widget_stepper", &suite); }
