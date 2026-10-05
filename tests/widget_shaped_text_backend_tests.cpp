#include "detail/shaped_text_backend.hpp"
#include "test_support.hpp"
#include <nativeui/paint.hpp>
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include <array>
#include <limits>

namespace {
auto raster() {
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 60));
  NUI_CHECK(surface);
  surface->getCanvas()->clear(SK_ColorWHITE);
  return surface;
}
std::vector<unsigned char> pixels(const sk_sp<SkSurface>& surface) {
  SkPixmap pixmap;
  NUI_CHECK(surface->peekPixels(&pixmap));
  const auto* begin = static_cast<const unsigned char*>(pixmap.addr());
  return {begin, begin + pixmap.computeByteSize()};
}
auto font() {
  auto fonts = ui::detail::capture_shaping_fonts();
  NUI_CHECK(fonts);
  const std::array<char32_t, 2> required{U'H', U'e'};
  auto resolved = ui::detail::resolve_shaping_font(*fonts, ui::TextStyle{}, required);
  NUI_CHECK(resolved && resolved->complete_coverage);
  NUI_CHECK(resolved->font.getTypeface());
  return resolved->font;
}
void final_glyph_geometry_is_painted_without_remeasurement() {
  const auto resolved = font();
  const std::array<SkGlyphID, 2> glyphs{
      resolved.unicharToGlyph(U'H'), resolved.unicharToGlyph(U'e')};
  // Descending clusters represent legal RTL glyph ordering.
  const std::array<std::uint32_t, 2> clusters{1, 0};
  const std::array<SkPoint, 2> positions{SkPoint{4, 22}, SkPoint{34, 38}};
  auto actual = raster(), expected = raster();
  actual->getCanvas()->translate(3, 2);
  expected->getCanvas()->translate(3, 2);
  const SkRect clip{0, 0, 74, 45};
  actual->getCanvas()->clipRect(clip);
  expected->getCanvas()->clipRect(clip);
  const int saves = actual->getCanvas()->getSaveCount();
  const auto matrix = actual->getCanvas()->getLocalToDevice();
  {
    ui::Painter painter{*actual->getCanvas()};
    ui::detail::ShapedTextPaintAccess::draw_glyph_run(
        painter, {1, 1}, resolved, glyphs, positions, clusters, "He", {0, 0, 0, 1});
  }
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorBLACK);
  expected->getCanvas()->drawGlyphs(
      SkSpan<const SkGlyphID>{glyphs.data(), glyphs.size()},
      SkSpan<const SkPoint>{positions.data(), positions.size()},
      SkSpan<const std::uint32_t>{clusters.data(), clusters.size()},
      SkSpan<const char>{"He", 2}, {1, 1}, resolved, paint);
  NUI_CHECK(pixels(actual) == pixels(expected));
  NUI_CHECK(actual->getCanvas()->getSaveCount() == saves);
  NUI_CHECK(actual->getCanvas()->getLocalToDevice() == matrix);
}
void invalid_runs_fail_before_drawing_and_do_not_poison_recovery() {
  const auto resolved = font();
  const std::array<SkGlyphID, 1> glyphs{resolved.unicharToGlyph(U'H')};
  const std::array<SkPoint, 1> positions{SkPoint{2, 20}};
  const std::array<std::uint32_t, 1> cluster{0};
  auto surface = raster();
  const auto before = pixels(surface);
  ui::Painter painter{*surface->getCanvas()};
  int failures{};
  auto must_fail = [&](auto invoke) {
    try { invoke(); } catch (const std::invalid_argument&) { ++failures; }
    NUI_CHECK(pixels(surface) == before);
  };
  must_fail([&] { ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, std::span<const SkPoint>{}, cluster, "H", {0, 0, 0, 1}); });
  must_fail([&] { const std::array<std::uint32_t, 1> invalid{1};
    ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, positions, invalid, "H", {0, 0, 0, 1}); });
  must_fail([&] { const std::array<std::uint32_t, 1> continuation{1};
    ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, positions, continuation, "é", {0, 0, 0, 1}); });
  must_fail([&] { const std::array<SkPoint, 1> invalid{SkPoint{0, std::numeric_limits<float>::infinity()}};
    ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, invalid, cluster, "H", {0, 0, 0, 1}); });
  must_fail([&] { ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, positions, cluster, "\xff", {0, 0, 0, 1}); });
  NUI_CHECK(failures == 5);
  ui::detail::ShapedTextPaintAccess::draw_glyph_run(
      painter, {}, resolved, glyphs, positions, cluster, "H", {0, 0, 0, 1});
  NUI_CHECK(pixels(surface) != before);
}
void suite() {
  final_glyph_geometry_is_painted_without_remeasurement();
  invalid_runs_fail_before_drawing_and_do_not_poison_recovery();
}
}
int main() { return test::run("widget_shaped_text_backend", &suite); }
