#include "test_support.hpp"
#include <array>
#include <limits>
#include <memory>
#include <nativeui/avatar.hpp>

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
void circle_image_and_fallback() {
  ui::State<ui::Image> image{ui::Image::decode(kWideRgbaPng)};
  NUI_CHECK(image.get().valid());
  ui::UI tree{ui::Avatar{"Camille Martin"}.image(image).size(32.0)};
  ui::HeadlessRenderer renderer{{64.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  const auto outside = renderer.pixel(0, 0);
  const auto left = renderer.pixel(25, 20);
  const auto right = renderer.pixel(39, 20);
  NUI_CHECK(left.g > left.r && left.g > left.b);
  NUI_CHECK(right.b > right.r && right.b > right.g);
  image.set({});
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(renderer.pixel(0, 0).r == outside.r);
  const auto background = renderer.pixel(32, 7);
  ui::UI same{ui::Avatar{"Camille Martin"}.size(32.0)};
  NUI_CHECK(renderer.render(same));
  const auto again = renderer.pixel(32, 7);
  NUI_CHECK(background.r == again.r && background.g == again.g &&
            background.b == again.b);
}
void source_lifetime_semantics_and_zero() {
  auto name = std::make_unique<ui::State<std::string>>("Camille Martin");
  ui::UI tree{ui::Avatar{name->binding()}.size(32.0).initials("")};
  ui::HeadlessRenderer renderer{{64.0f, 40.0f}, 1.0f};
  NUI_CHECK(renderer.render(tree));
  const auto before = renderer.pixel(32, 20);
  name.reset();
  NUI_CHECK(renderer.render(tree));
  const auto after = renderer.pixel(32, 20);
  NUI_CHECK(before.r == after.r && before.g == after.g && before.b == after.b);
  ui::detail::AvatarComponent empty{"", {}, {}, {}, 32.0, {}, {}};
  NUI_CHECK(empty.semantics().role == ui::SemanticRole::Image);
  NUI_CHECK(empty.semantics().description == "avatar sans nom");
  NUI_CHECK(!empty.focusable() && !empty.pointer_targetable());
  ui::detail::AvatarComponent repaired{
      std::string{"\xff"} + "Alice Martin", {}, {}, {}, 32.0, {}, {}};
  NUI_CHECK(repaired.semantics().name == "\xef\xbf\xbd"
                                         "Alice Martin");
  ui::UI zero{ui::Avatar{"Nobody"}.size(0.0)};
  NUI_CHECK(zero.measure().preferred.w == 0.0f &&
            zero.measure().preferred.h == 0.0f);
  NUI_CHECK(renderer.render(zero));
  NUI_CHECK(renderer.pixel(32, 20).r < 100 && renderer.pixel(32, 20).g < 100);
  bool rejected = false;
  try {
    auto invalid =
        ui::Avatar{"Invalid"}.size(std::numeric_limits<double>::infinity());
    (void)invalid;
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void suite() {
  circle_image_and_fallback();
  source_lifetime_semantics_and_zero();
}
} // namespace
int main() { return test::run("widget_avatar", &suite); }
