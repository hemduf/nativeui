#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/rating.hpp>

#include <stdexcept>

namespace {
void pointer_preview_half_steps_and_clear() {
  ui::State<double> value{1.0};
  ui::RatingStyle style;
  style.star_size = 20.0;
  style.gap = 0.0;
  ui::UI tree{ui::Rating{"Note", value, 5}.step(0.5).style(style)};
  test::MockPlatform platform;
  tree.resize({100.0f, 24.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 45.0f, 10.0f),
                platform);
  NUI_CHECK(value.get() == 1.0);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 45.0f, 10.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 45.0f, 10.0f),
                platform);
  NUI_CHECK(value.get() == 2.5);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 45.0f, 10.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 45.0f, 10.0f),
                platform);
  NUI_CHECK(value.get() == 0.0);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void keyboard_boundaries_throw_recovery_and_empty() {
  ui::State<double> value{1.0};
  bool fail = true;
  auto subscription = value.observe([&](double) {
    if (fail)
      throw std::runtime_error("injected rating observer");
  });
  ui::UI tree{ui::Rating{"Note", value, 5}.step(0.5)};
  test::MockPlatform platform;
  tree.resize({150.0f, 30.0f});
  tree.activate(platform);
  bool caught = false;
  try {
    tree.dispatch(test::key(ui::Key::Right), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == 1.5);
  fail = false;
  tree.dispatch(test::key(ui::Key::End), platform);
  NUI_CHECK(value.get() == 5.0);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 5.0);
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(value.get() == 0.0);
  ui::UI empty{ui::Rating{"Empty", value, 0}};
  NUI_CHECK(empty.measure().preferred.w == 0.0f);
  ui::HeadlessRenderer renderer{{150.0f, 30.0f}, 1.0f};
  NUI_CHECK(renderer.render(empty));
}
void fractional_fill_is_clipped_per_star() {
  ui::State<double> value{2.5};
  ui::RatingStyle style;
  style.star_size = 20.0;
  style.gap = 0.0;
  style.outline_width = 0.0;
  style.empty = ui::Color{0.0f, 0.0f, 0.0f, 1.0f};
  style.filled = ui::Color{1.0f, 0.0f, 0.0f, 1.0f};
  ui::UI tree{ui::Rating{"Note", value, 5}.step(0.5).style(style)};
  ui::HeadlessRenderer renderer{{100.0f, 24.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(renderer.pixel(47, 12).r > 240);
  NUI_CHECK(renderer.pixel(53, 12).r < 10);
}
void hover_leaves_to_sibling_restores_model_pixels() {
  ui::State<double> value{1.0};
  ui::RatingStyle style;
  style.star_size = 20.0;
  style.gap = 0.0;
  style.outline_width = 0.0;
  style.empty = ui::Color{0, 0, 0, 1};
  style.filled = ui::Color{1, 0, 0, 1};
  style.preview = ui::Color{1, 0, 0, 1};
  ui::UI tree{ui::Row{ui::Rating{"Note", value, 5}.style(style),
                      ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({200, 24});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 24));
  NUI_CHECK(surface);
  auto red_at_third_star = [&] {
    tree.paint(*surface->getCanvas(), platform);
    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    return SkColorGetR(pixels.getColor(50, 12));
  };
  NUI_CHECK(red_at_third_star() < 10);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 50, 12), platform);
  NUI_CHECK(value.get() == 1.0 && red_at_third_star() > 240);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 140, 12), platform);
  NUI_CHECK(value.get() == 1.0 && red_at_third_star() < 10);
}
void suite() {
  hover_leaves_to_sibling_restores_model_pixels();
  fractional_fill_is_clipped_per_star();
  pointer_preview_half_steps_and_clear();
  keyboard_boundaries_throw_recovery_and_empty();
}
} // namespace
int main() { return test::run("widget_rating", &suite); }
