#include "../src/detail/widget_svg_tint.hpp"
#include "test_support.hpp"
#include <nativeui/icon_view.hpp>

#include "include/core/SkSurface.h"

#include <limits>
#include <string_view>

namespace ui::detail {
struct PainterSvgFaultAccess {
  static void fail_after_transform(Painter &painter) noexcept {
    painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterSvgTransform;
  }
};
} // namespace ui::detail
namespace {
ui::SvgIcon fixture() {
  constexpr std::string_view source =
      R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="2"><rect x="0" width="2" height="2" fill="#ff0000"/><rect x="2" width="2" height="2" fill="#0000ff" opacity="0.5"/></svg>)";
  auto result = ui::SvgIcon::parse(
      {reinterpret_cast<const std::byte *>(source.data()), source.size()});
  NUI_CHECK(result.valid());
  return result;
}
void tint_alpha_and_native_colors() {
  const auto icon = fixture();
  ui::HeadlessRenderer renderer{{20.0f, 10.0f}, 1.0f};
  ui::UI green{ui::IconView{icon}.size(10.0).color({0.0f, 1.0f, 0.0f, 1.0f})};
  NUI_CHECK(renderer.render(green));
  NUI_CHECK(renderer.pixel(3, 5).g > 240 && renderer.pixel(3, 5).r < 10);
  const auto semi = renderer.pixel(17, 5);
  NUI_CHECK(semi.g > 100 && semi.g < 180 && semi.r < 50 && semi.b < 50);
  ui::UI red{ui::IconView{icon}.size(10.0).color({1.0f, 0.0f, 0.0f, 1.0f})};
  NUI_CHECK(renderer.render(red));
  NUI_CHECK(renderer.pixel(3, 5).r > 240 && renderer.pixel(3, 5).g < 10);
  NUI_CHECK(renderer.render(green));
  NUI_CHECK(renderer.pixel(3, 5).g > 240 && renderer.pixel(3, 5).r < 10);
  ui::UI native{ui::IconView{icon}
                    .size(10.0)
                    .color({0.0f, 1.0f, 0.0f, 1.0f})
                    .monochrome(false)};
  NUI_CHECK(renderer.render(native));
  NUI_CHECK(renderer.pixel(3, 5).r > 240 && renderer.pixel(3, 5).g < 10);
  NUI_CHECK(renderer.pixel(17, 5).b > 100 && renderer.pixel(17, 5).g < 50);
  ui::UI translucent{ui::IconView{icon}.size(10.0).color({0, 1, 0, 0.5f})};
  NUI_CHECK(renderer.render(translucent));
  const auto opaque_shape = renderer.pixel(3, 5);
  const auto translucent_shape = renderer.pixel(17, 5);
  NUI_CHECK(opaque_shape.g > 100 && opaque_shape.g < 180);
  NUI_CHECK(translucent_shape.g > 60 && translucent_shape.g < 115);
}
void layout_semantics_and_invalid_resource() {
  const auto icon = fixture();
  ui::State<ui::SvgIcon> source{icon};
  ui::UI tree{ui::IconView{source}.size(10.0).alt("Saved")};
  NUI_CHECK(tree.measure().preferred.w == 20.0f &&
            tree.measure().preferred.h == 10.0f);
  source.set({});
  NUI_CHECK(tree.measure().preferred.w == 10.0f &&
            tree.measure().preferred.h == 10.0f);
  ui::detail::IconViewComponent informative{icon, {},      10.0, {},
                                            true, "Saved", false};
  NUI_CHECK(informative.semantics().role == ui::SemanticRole::Image);
  NUI_CHECK(informative.semantics().name == "Saved");
  bool rejected = false;
  try {
    auto invalid = ui::IconView{icon}.color(
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f});
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void tint_backend_fault_restores_parent() {
  const auto icon = fixture();
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 24));
  NUI_CHECK(surface);
  auto &canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  canvas.translate(2.0f, 3.0f);
  const auto matrix = canvas.getTotalMatrix();
  const auto count = canvas.getSaveCount();
  ui::Painter painter{canvas};
  ui::detail::PainterSvgFaultAccess::fail_after_transform(painter);
  bool caught = false;
  try {
    ui::detail::draw_svg_monochrome(painter, icon, {0, 0, 20, 10},
                                    {0, 1, 0, 1});
  } catch (const std::bad_alloc &) {
    caught = true;
  }
  NUI_CHECK(caught);
  NUI_CHECK(canvas.getSaveCount() == count &&
            canvas.getTotalMatrix() == matrix);
  NUI_CHECK(painter.save_depth() == 0);
  ui::detail::draw_svg_monochrome(painter, icon, {0, 0, 20, 10}, {0, 1, 0, 1});
  painter.fill_rounded_rect({22, 0, 4, 4}, 0, {0, 0, 1, 1});
  SkPixmap pixels;
  NUI_CHECK(surface->peekPixels(&pixels));
  NUI_CHECK(pixels.getColor(5, 8) == SK_ColorGREEN);
  NUI_CHECK(pixels.getColor(25, 4) == SK_ColorBLUE);
}
void suite() {
  tint_alpha_and_native_colors();
  layout_semantics_and_invalid_resource();
  tint_backend_fault_restores_parent();
}
} // namespace
int main() { return test::run("widget_icon_view", &suite); }
