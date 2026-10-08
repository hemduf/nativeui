#include "test_support.hpp"
#include <nativeui/button.hpp>
#include <nativeui/canvas.hpp>

namespace {
void variant_style_and_content() {
  ui::ButtonStyle style;
  style.base.fill = ui::Color{0.0f, 1.0f, 0.0f, 1.0f};
  style.base.corner_radius = 0.0f;
  style.base.border_width = 0.0f;
  ui::UI primary{
      ui::Button{"Apply", {}}.variant(ui::ButtonVariant::Primary).style(style)};
  ui::HeadlessRenderer renderer{{120.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(primary));
  const auto custom = renderer.pixel(10, 10);
  NUI_CHECK(custom.r == 0 && custom.g == 255 && custom.b == 0);
  ui::UI toolbar{ui::Button{"Tools", {}}.variant(ui::ButtonVariant::Toolbar)};
  NUI_CHECK(renderer.render(toolbar));
  const auto background = renderer.pixel(10, 10);
  NUI_CHECK(background.g < 100);

  int activations{};
  ui::UI composed{ui::Button{"Named icon action", [&] { ++activations; }}
                      .content(ui::Canvas{
                          30.0f, 20.0f, [](ui::CanvasContext2D &g) {
                            g.fill_rect({0.0f, 0.0f, g.width(), g.height()},
                                        {0.0f, 0.0f, 1.0f, 1.0f});
                          }}.spec())};
  composed.resize({120.0f, 40.0f});
  test::MockPlatform platform;
  composed.activate(platform);
  composed.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(activations == 1);
  NUI_CHECK(renderer.render(composed));
  const auto center = renderer.pixel(60, 20);
  NUI_CHECK(center.b == 255 && center.r == 0 && center.g == 0);
}
void rejects_interactive_descendants() {
  bool rejected = false;
  try {
    ui::UI invalid{ui::Button{"Parent", {}}.content(
        ui::Column{ui::Label{"Decoration"}, ui::Button{"Nested", {}}}.spec())};
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
  ui::UI recovery{ui::Button{"Recovery", {}}.content(ui::Label{"Safe"}.spec())};
  ui::HeadlessRenderer renderer{{120.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(recovery));
}
void suite() {
  variant_style_and_content();
  rejects_interactive_descendants();
}
} // namespace
int main() { return test::run("widget_button_variants", &suite); }
