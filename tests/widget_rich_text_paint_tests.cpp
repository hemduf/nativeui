#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/rich_text.hpp>
namespace {
std::vector<SkColor> pixels(ui::UI &tree) {
  test::MockPlatform platform;
  tree.resize({220, 90});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(220, 90));
  NUI_CHECK(surface);
  auto *canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->save();
  canvas->translate(3, 4);
  canvas->clipRect(SkRect::MakeXYWH(0, 0, 210, 80));
  const auto before = canvas->getTotalMatrix();
  const auto save_count = canvas->getSaveCount();
  tree.paint(*canvas, platform);
  NUI_CHECK(canvas->getTotalMatrix() == before &&
            canvas->getSaveCount() == save_count);
  canvas->restore();
  SkPixmap map;
  NUI_CHECK(surface->peekPixels(&map));
  std::vector<SkColor> result;
  for (int y = 0; y < 90; ++y)
    for (int x = 0; x < 220; ++x)
      result.push_back(map.getColor(x, y));
  return result;
}
void decoration_boundary_does_not_reform_the_paragraph() {
  ui::TextStyle style;
  style.size = 24;
  style.color = {0, 0, 1, 1};
  style.family = "serif";
  ui::UI one{
      ui::RichText{std::vector<ui::RichTextSpan>{{.text = "office العربية"}}}
          .style(style)};
  ui::UI split{ui::RichText{
      std::vector<ui::RichTextSpan>{{.text = "of"},
                                    {.text = "fice العربية", .style = style}}}
                   .style(style)};
  NUI_CHECK(pixels(one) == pixels(split));
}
void mixed_background_color_underline_and_strike_render() {
  ui::TextStyle red;
  red.size = 20;
  red.color = {1, 0, 0, 1};
  ui::TextStyle blue = red;
  blue.color = {0, 0, 1, 1};
  blue.weight = ui::FontWeight::Bold;
  ui::UI decorated{
      ui::RichText{std::vector<ui::RichTextSpan>{
                       {.text = "Red",
                        .style = red,
                        .background = ui::Color{0, 1, 0, 1},
                        .underline = true},
                       {.text = " Blue", .style = blue, .strikethrough = true}}}
          .style(red)};
  const auto image = pixels(decorated);
  int r{}, g{}, b{};
  for (const auto color : image) {
    if (SkColorGetR(color) > 200 && SkColorGetG(color) < 40 &&
        SkColorGetB(color) < 40)
      ++r;
    if (SkColorGetG(color) > 200 && SkColorGetR(color) < 40 &&
        SkColorGetB(color) < 40)
      ++g;
    if (SkColorGetB(color) > 200 && SkColorGetR(color) < 40 &&
        SkColorGetG(color) < 40)
      ++b;
  }
  NUI_CHECK(r > 0 && g > 0 && b > 0);
}
void suite() {
  decoration_boundary_does_not_reform_the_paragraph();
  mixed_background_color_underline_and_strike_render();
}
} // namespace
int main() { return test::run("rich_text_paint", suite); }
