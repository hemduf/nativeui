#include <nativeui/button.hpp>
#include <nativeui/canvas.hpp>
#include <nativeui/checkbox.hpp>
#include <nativeui/header.hpp>
#include <nativeui/knob.hpp>
#include <nativeui/label.hpp>
#include <nativeui/meter.hpp>
#include <nativeui/progress_bar.hpp>
#include <nativeui/range_slider.hpp>
#include <nativeui/slider.hpp>
#include <nativeui/text_area.hpp>
#include <nativeui/text_input.hpp>
#include <nativeui/toggle.hpp>

#include "test_support.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <limits>
#include <memory>
#include <stdexcept>

namespace {

ui::InputEvent key_up(ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  return event;
}

void legacy_activation_timings() {
  test::MockPlatform platform;
  int activations = 0;
  ui::UI button{ui::Button{"Action", [&] { ++activations; }}};
  button.resize({200.0f, 60.0f});
  button.activate(platform);
  button.dispatch(test::key(ui::Key::Space), platform);
  button.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(activations == 0);
  button.dispatch(key_up(ui::Key::Space), platform);
  NUI_CHECK(activations == 1);
  button.dispatch(test::key(ui::Key::Enter), platform);
  button.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(activations == 2);

  ui::State<bool> checked{false};
  ui::UI checkbox{ui::Checkbox{checked, "Choice"}};
  checkbox.resize({200.0f, 60.0f});
  checkbox.activate(platform);
  checkbox.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(!checked.get());
  checked.set(true);
  checkbox.dispatch(key_up(ui::Key::Space), platform);
  NUI_CHECK(!checked.get());
  checkbox.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!checked.get());

  ui::State<bool> toggled{false};
  ui::UI toggle{ui::Toggle{"Switch", toggled}};
  toggle.resize({210.0f, 54.0f});
  toggle.activate(platform);
  toggle.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(toggled.get());
  toggle.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(toggled.get());
  toggle.dispatch(key_up(ui::Key::Space), platform);
  NUI_CHECK(toggled.get());
}

void independent_components_and_throw_recovery() {
  test::MockPlatform platform;
  bool should_throw = true;
  int first_calls = 0;
  int second_calls = 0;
  auto first = std::make_unique<ui::UI>(
      ui::Button{"First", [&] {
                   ++first_calls;
                   if (should_throw)
                     throw std::runtime_error("injected activation failure");
                 }});
  ui::UI second{ui::Button{"Second", [&] { ++second_calls; }}};
  first->resize({180.0f, 40.0f});
  second.resize({180.0f, 40.0f});
  first->activate(platform);
  second.activate(platform);
  bool caught = false;
  try {
    first->dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught);
  should_throw = false;
  first->dispatch(key_up(ui::Key::Enter), platform);
  first->dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(first_calls == 2);
  first.reset();
  second.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(second_calls == 1);
}

void canvas_exception_restores_local_scopes() {
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 60));
  NUI_CHECK(surface);
  surface->getCanvas()->clear(SK_ColorBLACK);
  ui::Painter painter{*surface->getCanvas()};
  test::MockPlatform platform;
  ui::CanvasComponent throwing{
      {10.0f, 10.0f},
      [](ui::CanvasContext2D &g) {
        g.fill_rect({0.0f, 0.0f, 10.0f, 10.0f}, ui::colors::accent);
        throw std::runtime_error("injected canvas draw failure");
      },
      {},
      false};
  ui::PaintContext first{
      painter, {10.0f, 10.0f, 10.0f, 10.0f}, false, platform};
  bool caught = false;
  try {
    throwing.paint(first);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught);
  NUI_CHECK(painter.save_depth() == 0);
  ui::CanvasComponent normal{{10.0f, 10.0f},
                             [](ui::CanvasContext2D &g) {
                               g.fill_rect({0.0f, 0.0f, g.width(), g.height()},
                                           {0.0f, 0.0f, 1.0f, 1.0f});
                             },
                             {},
                             false};
  ui::PaintContext second{
      painter, {70.0f, 20.0f, 10.0f, 10.0f}, false, platform};
  normal.paint(second);
  NUI_CHECK(painter.save_depth() == 0);
  SkPixmap pixels;
  NUI_CHECK(surface->peekPixels(&pixels));
  NUI_CHECK(SkColorGetB(pixels.getColor(75, 25)) == 255);
}

void canvas_validates_dimensions() {
  ui::CanvasComponent repaired{
      {std::numeric_limits<float>::quiet_NaN(), -2.0f}, {}, {}, false};
  const auto measured = repaired.measure({});
  NUI_CHECK(measured.w == 1.0f && measured.h == 1.0f);
  bool rejected = false;
  try {
    ui::CanvasComponent invalid{
        {std::numeric_limits<float>::infinity(), 10.0f}, {}, {}, false};
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}

void header_custom_content_and_label_semantics() {
  ui::UI legacy{ui::Header{"Title"}};
  const auto old_size = legacy.measure();
  NUI_CHECK(old_size.preferred.w == 640.0f && old_size.preferred.h == 70.0f);
  ui::UI with_subtitle{ui::Header{"Title"}.subtitle("Description")};
  ui::UI without_subtitle{ui::Header{"Title"}.subtitle("")};
  NUI_CHECK(with_subtitle.measure().preferred.h >
            without_subtitle.measure().preferred.h);
  ui::LabelComponent label{"Owned label", {}};
  NUI_CHECK(label.semantics().role == ui::SemanticRole::Text);
  NUI_CHECK(label.semantics().text_value ==
            std::optional<std::string>{"Owned label"});
}

void knob_external_write_cancels_drag() {
  test::MockPlatform platform;
  ui::State<float> value{0.5f};
  ui::UI knob{ui::Knob{"Value", value}};
  knob.resize({176.0f, 182.0f});
  knob.activate(platform);
  knob.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                platform);
  knob.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 82.0f),
                platform);
  NUI_CHECK_NEAR(value.get(), 0.6f, 0.0001f);
  knob.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 64.0f),
                platform);
  NUI_CHECK_NEAR(value.get(), 0.7f, 0.0001f);
  value.set(0.2f);
  knob.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 46.0f),
                platform);
  NUI_CHECK_NEAR(value.get(), 0.2f, 0.0001f);
  knob.dispatch(test::pointer(ui::InputType::PointerUp, 80.0f, 46.0f),
                platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  knob.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK_NEAR(value.get(), 0.21f, 0.0001f);
}

void display_bindings_outlive_source() {
  auto source = std::make_unique<ui::State<float>>(0.8f);
  ui::UI progress{ui::ProgressBar{source->binding()}.reversed()};
  ui::UI meter{ui::Meter{source->binding()}.levels({0.5, 0.75})};
  source.reset();
  ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};
  NUI_CHECK(renderer.render(progress));
  NUI_CHECK(renderer.render(meter));
}

void meter_levels_and_disabled_priority() {
  ui::State<float> value{0.5f};
  ui::MeterStyle style;
  style.base.track = ui::Color{0.0f, 0.0f, 0.0f, 1.0f};
  style.base.fill = ui::Color{0.0f, 0.0f, 1.0f, 1.0f};
  style.base.border_width = 0.0f;
  style.base.corner_radius = 0.0f;
  style.base.fill_corner_radius = 0.0f;
  style.disabled.fill = ui::Color{0.0f, 1.0f, 0.0f, 1.0f};
  ui::UI meter{
      ui::Meter{value}
          .levels({0.5, 0.75})
          .threshold_colors({1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f})
          .style(style)};
  ui::HeadlessRenderer renderer{{100.0f, 20.0f}, 1.0f};
  NUI_CHECK(renderer.render(meter));
  auto pixel = renderer.pixel(10, 10);
  NUI_CHECK(pixel.r == 255 && pixel.g == 255 && pixel.b == 0);
  value.set(0.75f);
  NUI_CHECK(renderer.render(meter));
  pixel = renderer.pixel(10, 10);
  NUI_CHECK(pixel.r == 255 && pixel.g == 0 && pixel.b == 0);

  ui::State<bool> enabled{false};
  ui::UI disabled{
      ui::Enabled{enabled, ui::Meter{value}
                               .levels({0.5, 0.75})
                               .threshold_colors({1.0f, 1.0f, 0.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f, 1.0f})
                               .style(style)}};
  NUI_CHECK(renderer.render(disabled));
  pixel = renderer.pixel(10, 10);
  NUI_CHECK(pixel.r == 0 && pixel.g == 255 && pixel.b == 0);

  ui::detail::BoundedDisplayComponent low_bad{
      value.binding(),
      0.0f,
      1.0f,
      ui::ProgressOrientation::Horizontal,
      {},
      style,
      ui::MeterLevels{0.5, 0.25}};
  value.set(0.25f);
  NUI_CHECK(low_bad.semantics().description == "critical");
  value.set(0.5f);
  NUI_CHECK(low_bad.semantics().description == "warning");
  value.set(0.75f);
  NUI_CHECK(low_bad.semantics().description == "normal");
  ui::detail::BoundedDisplayComponent equal{value.binding(),
                                            0.0f,
                                            1.0f,
                                            ui::ProgressOrientation::Horizontal,
                                            {},
                                            style,
                                            ui::MeterLevels{0.5, 0.5}};
  NUI_CHECK(equal.semantics().description.empty());

  bool rejected = false;
  try {
    auto invalid = ui::Meter{value}.levels({0.5, 1.5});
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}

void text_submit_recovers_after_throw() {
  ui::State<std::string> text{"entry"};
  bool should_throw = true;
  int submits{};
  ui::UI input{
      ui::TextInput{"Input", text}.on_submit([&](const std::string &value) {
        ++submits;
        NUI_CHECK(value == text.get());
        if (should_throw)
          throw std::runtime_error("injected submit failure");
      })};
  test::MockPlatform platform;
  input.resize({280.0f, 90.0f});
  input.activate(platform);
  bool caught = false;
  try {
    input.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught);
  should_throw = false;
  input.dispatch(test::text("!"), platform);
  input.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(submits == 2);
  NUI_CHECK(text.get().find('!') != std::string::npos);
}

void range_keyboard_changes_active_handle_only() {
  ui::State<ui::RangeValue> value{{0.2f, 0.8f}};
  ui::UI range{ui::RangeSlider{value}.step(0.1f)};
  test::MockPlatform platform;
  range.resize({200.0f, 60.0f});
  range.activate(platform);
  range.dispatch(test::key(ui::Key::Enter), platform);
  range.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == (ui::RangeValue{0.2f, 0.8f}));
  range.dispatch(test::key(ui::Key::Left), platform);
  NUI_CHECK_NEAR(value.get().low, 0.2f, 0.0001f);
  NUI_CHECK_NEAR(value.get().high, 0.7f, 0.0001f);
}

void suite() {
  legacy_activation_timings();
  independent_components_and_throw_recovery();
  canvas_exception_restores_local_scopes();
  canvas_validates_dimensions();
  header_custom_content_and_label_semantics();
  knob_external_write_cancels_drag();
  display_bindings_outlive_source();
  meter_levels_and_disabled_priority();
  text_submit_recovers_after_throw();
  range_keyboard_changes_active_handle_only();
}

} // namespace

int main() { return test::run("widget_controls_extraction", &suite); }
