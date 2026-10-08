#include "test_support.hpp"
#include <nativeui/badge.hpp>
#include <nativeui/divider.hpp>
#include <nativeui/image_view.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>

namespace {
constexpr std::array<std::byte, 78> kWideRgbaPng{
    std::byte{137}, std::byte{80},  std::byte{78},  std::byte{71},
    std::byte{13},  std::byte{10},  std::byte{26},  std::byte{10},
    std::byte{0},   std::byte{0},   std::byte{0},   std::byte{13},
    std::byte{73},  std::byte{72},  std::byte{68},  std::byte{82},
    std::byte{0},   std::byte{0},   std::byte{0},   std::byte{4},
    std::byte{0},   std::byte{0},   std::byte{0},   std::byte{2},
    std::byte{8},   std::byte{6},   std::byte{0},   std::byte{0},
    std::byte{0},   std::byte{127}, std::byte{168}, std::byte{125},
    std::byte{99},  std::byte{0},   std::byte{0},   std::byte{0},
    std::byte{21},  std::byte{73},  std::byte{68},  std::byte{65},
    std::byte{84},  std::byte{120}, std::byte{218}, std::byte{99},
    std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31},  std::byte{12},  std::byte{25},  std::byte{254},
    std::byte{131}, std::byte{1},   std::byte{3},   std::byte{186},
    std::byte{0},   std::byte{0},   std::byte{52},  std::byte{251},
    std::byte{19},  std::byte{237}, std::byte{251}, std::byte{179},
    std::byte{56},  std::byte{239}, std::byte{0},   std::byte{0},
    std::byte{0},   std::byte{0},   std::byte{73},  std::byte{69},
    std::byte{78},  std::byte{68},  std::byte{174}, std::byte{66},
    std::byte{96},  std::byte{130},
};

std::span<const std::byte> bytes(std::string_view value) {
  return {reinterpret_cast<const std::byte *>(value.data()), value.size()};
}
constexpr std::string_view kWideSvg =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="2" viewBox="0 0 4 2">
<rect x="0" width="1" height="2" fill="#ff0000"/><rect x="1" width="1" height="2" fill="#00ff00"/><rect x="2" width="1" height="2" fill="#0000ff"/><rect x="3" width="1" height="2" fill="#ffffff"/></svg>)svg";

void divider_geometry_and_input() {
  ui::detail::DividerComponent horizontal{ui::DividerOrientation::Horizontal,
                                          3.0,
                                          ui::Color{1.0f, 0.0f, 0.0f, 1.0f}};
  ui::detail::DividerComponent vertical{
      ui::DividerOrientation::Vertical, 2.0, {}};
  NUI_CHECK(horizontal.measure({}).w == 0.0f &&
            horizontal.measure({}).h == 3.0f);
  NUI_CHECK(vertical.measure({}).w == 2.0f && vertical.measure({}).h == 0.0f);
  NUI_CHECK(!horizontal.focusable() && !horizontal.pointer_targetable());
  NUI_CHECK(horizontal.semantics().role == ui::SemanticRole::None);
  ui::UI tree{ui::Divider{}.thickness(3.0).color({1.0f, 0.0f, 0.0f, 1.0f})};
  ui::HeadlessRenderer renderer{{12.0f, 6.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  const auto line = renderer.pixel(8, 1);
  const auto outside = renderer.pixel(8, 5);
  NUI_CHECK(line.r == 255 && line.g == 0 && line.b == 0);
  NUI_CHECK(outside.r != 255);
  bool rejected = false;
  try {
    auto invalid =
        ui::Divider{}.thickness(std::numeric_limits<double>::infinity());
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}

void badge_layout_state_and_lifetime() {
  auto source = std::make_unique<ui::State<std::string>>("3");
  ui::detail::BadgeComponent component{"", source->binding(), {}};
  NUI_CHECK(component.semantics().text_value ==
            std::optional<std::string>{"3"});
  ui::UI tree{ui::Badge{source->binding()}};
  ui::HeadlessRenderer renderer{{80.0f, 24.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  const float before = tree.measure().preferred.w;
  source->set("12345");
  NUI_CHECK(tree.layout_dirty());
  NUI_CHECK(tree.measure().preferred.w > before);
  NUI_CHECK(component.semantics().text_value ==
            std::optional<std::string>{"12345"});
  source.reset();
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(component.semantics().text_value ==
            std::optional<std::string>{"12345"});
  NUI_CHECK(!component.focusable() && !component.pointer_targetable());
  ui::BadgeStyle style;
  style.horizontal_padding = -1.0;
  bool rejected = false;
  try {
    auto invalid = ui::Badge{"bad"}.style(style);
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}

void image_view_intrinsic_and_fit() {
  const auto raster = ui::Image::decode(kWideRgbaPng);
  const auto svg = ui::SvgIcon::parse(bytes(kWideSvg));
  NUI_CHECK(raster.valid() && svg.valid());
  ui::UI natural{ui::ImageView{raster}.pixel_scale(2.0)};
  NUI_CHECK(natural.measure().preferred.w == 2.0f &&
            natural.measure().preferred.h == 1.0f);
  ui::UI natural_svg{ui::ImageView{svg}.pixel_scale(2.0)};
  NUI_CHECK(natural_svg.measure().preferred.w == 4.0f &&
            natural_svg.measure().preferred.h == 2.0f);
  ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
  for (const auto fit :
       {ui::ImageFit::Contain, ui::ImageFit::Fill, ui::ImageFit::Cover}) {
    for (const auto &source :
         {ui::ImageView::Source{svg}, ui::ImageView::Source{raster}}) {
      ui::State<ui::ImageView::Source> value{source};
      ui::UI tree{ui::ImageView{value}.fit(fit).size({8.0f, 8.0f})};
      NUI_CHECK(renderer.render(tree));
      const auto edge = renderer.pixel(4, 1);
      // Raster fit uses linear sampling. Probe the clamped outer texels so
      // the solid-color oracle is independent of interpolated inner edges.
      const auto left = renderer.pixel(0, 4);
      const auto right = renderer.pixel(7, 4);
      if (fit == ui::ImageFit::Contain) {
        NUI_CHECK(edge.r < 100 && edge.g < 100 && edge.b < 100);
        NUI_CHECK(left.r > 200 && left.g < 40 && left.b < 40);
      } else if (fit == ui::ImageFit::Fill) {
        NUI_CHECK(left.r > 200 && left.g < 40 && left.b < 40);
        NUI_CHECK(right.r > 200 && right.g > 200 && right.b > 200);
      } else {
        NUI_CHECK(left.g > left.r && left.g > left.b);
        NUI_CHECK(right.b > right.r && right.b > right.g);
      }
    }
  }
  ui::detail::ImageViewComponent informative{
      raster, {}, ui::ImageFit::Contain, {}, 1.0, "Prepared preview", false};
  NUI_CHECK(informative.semantics().role == ui::SemanticRole::Image);
  NUI_CHECK(informative.semantics().name == "Prepared preview");
}

void image_view_source_swap_and_lifetime() {
  auto source = std::make_unique<ui::State<ui::ImageView::Source>>(ui::Image{});
  ui::UI tree{ui::ImageView{source->binding()}};
  ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.measure().preferred.w == 0.0f);
  source->set(ui::SvgIcon::parse(bytes(kWideSvg)));
  NUI_CHECK(tree.layout_dirty());
  NUI_CHECK(tree.measure().preferred.w == 4.0f);
  source.reset();
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(renderer.pixel(4, 4).b > 200);
  bool rejected = false;
  try {
    auto invalid = ui::ImageView{ui::Image{}}.pixel_scale(0.0);
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void suite() {
  divider_geometry_and_input();
  badge_layout_state_and_lifetime();
  image_view_intrinsic_and_fit();
  image_view_source_swap_and_lifetime();
}
} // namespace
int main() { return test::run("widget_leaf", &suite); }
