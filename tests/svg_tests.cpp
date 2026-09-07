#include "test_support.hpp"

#include <nativeui/svg.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace {

constexpr std::string_view kQuadrantSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2">
  <rect x="0" y="0" width="1" height="1" fill="#ff0000"/>
  <rect x="1" y="0" width="1" height="1" fill="#00ff00"/>
  <rect x="0" y="1" width="1" height="1" fill="#0000ff"/>
  <rect x="1" y="1" width="1" height="1" fill="#ffffff"/>
</svg>)svg";

std::span<const std::byte> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

void parse_and_render_at_logical_size() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kQuadrantSvg));
    NUI_CHECK(icon.valid());
    NUI_CHECK(icon.intrinsic_size().w == 2.0f);
    NUI_CHECK(icon.intrinsic_size().h == 2.0f);

    ui::UI tree{ui::Canvas{16.0f, 16.0f, [icon](ui::CanvasContext2D& canvas) {
        canvas.draw_svg(icon, {0.0f, 0.0f, 16.0f, 16.0f});
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto red = renderer.pixel(2, 2);
    const auto green = renderer.pixel(13, 2);
    const auto blue = renderer.pixel(2, 13);
    const auto white = renderer.pixel(13, 13);
    NUI_CHECK(red.r > 220 && red.g < 30 && red.b < 30);
    NUI_CHECK(green.g > 220 && green.r < 30 && green.b < 30);
    NUI_CHECK(blue.b > 220 && blue.r < 30 && blue.g < 30);
    NUI_CHECK(white.r > 220 && white.g > 220 && white.b > 220);
}

void suite() {
    parse_and_render_at_logical_size();
}

} // namespace

int main() { return test::run("svg", &suite); }
