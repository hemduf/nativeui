#include "test_support.hpp"

#include <nativeui/image.hpp>
#include <nativeui/svg.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kQuadrantSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2">
  <rect x="0" y="0" width="1" height="1" fill="#ff0000"/>
  <rect x="1" y="0" width="1" height="1" fill="#00ff00"/>
  <rect x="0" y="1" width="1" height="1" fill="#0000ff"/>
  <rect x="1" y="1" width="1" height="1" fill="#ffffff"/>
</svg>)svg";

constexpr std::string_view kViewBoxOnlySvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 2 2">
  <rect x="0" y="0" width="1" height="1" fill="#ff0000"/>
  <rect x="1" y="0" width="1" height="1" fill="#00ff00"/>
  <rect x="0" y="1" width="1" height="1" fill="#0000ff"/>
  <rect x="1" y="1" width="1" height="1" fill="#ffffff"/>
</svg>)svg";

std::span<const std::byte> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

std::vector<std::byte> byte_vector(std::string_view text) {
    const auto bytes = as_bytes(text);
    return {bytes.begin(), bytes.end()};
}

class CountingProvider final : public ui::ResourceProvider {
public:
    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) override {
        ++calls;
        if (resource_id == "good") return byte_vector(kQuadrantSvg);
        if (resource_id == "bad") return std::vector<std::byte>{};
        return std::nullopt;
    }

    int calls{};
};

void check_quadrants(const ui::HeadlessRenderer& renderer) {
    const auto red = renderer.pixel(2, 2);
    const auto green = renderer.pixel(13, 2);
    const auto blue = renderer.pixel(2, 13);
    const auto white = renderer.pixel(13, 13);
    NUI_CHECK(red.r > 220 && red.g < 30 && red.b < 30);
    NUI_CHECK(green.g > 220 && green.r < 30 && green.b < 30);
    NUI_CHECK(blue.b > 220 && blue.r < 30 && blue.g < 30);
    NUI_CHECK(white.r > 220 && white.g > 220 && white.b > 220);
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
    check_quadrants(renderer);
}

void default_fit_preserves_intrinsic_aspect_ratio() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kQuadrantSvg));
    NUI_CHECK(icon.valid());

    ui::UI tree{ui::Canvas{16.0f, 8.0f, [icon](ui::CanvasContext2D& canvas) {
        canvas.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, ui::colors::background);
        canvas.draw_svg(icon, {0.0f, 0.0f, 16.0f, 8.0f});
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    // A square SVG in a 2:1 destination must use centered contain-fit by
    // default. With the old independent X/Y scale these margin pixels become
    // SVG colors instead of remaining background.
    const auto left_margin = renderer.pixel(1, 1);
    const auto right_margin = renderer.pixel(14, 6);
    NUI_CHECK(left_margin.r < 40 && left_margin.g < 40 && left_margin.b < 40);
    NUI_CHECK(right_margin.r < 40 && right_margin.g < 40 && right_margin.b < 40);

    const auto red = renderer.pixel(5, 1);
    const auto green = renderer.pixel(10, 1);
    const auto blue = renderer.pixel(5, 6);
    const auto white = renderer.pixel(10, 6);
    NUI_CHECK(red.r > 220 && red.g < 30 && red.b < 30);
    NUI_CHECK(green.g > 220 && green.r < 30 && green.b < 30);
    NUI_CHECK(blue.b > 220 && blue.r < 30 && blue.g < 30);
    NUI_CHECK(white.r > 220 && white.g > 220 && white.b > 220);
}

void viewbox_only_icons_use_their_viewbox_as_intrinsic_size() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kViewBoxOnlySvg));
    NUI_CHECK(icon.valid());
    NUI_CHECK(icon.intrinsic_size().w == 2.0f);
    NUI_CHECK(icon.intrinsic_size().h == 2.0f);

    ui::UI tree{ui::Canvas{16.0f, 16.0f, [icon](ui::CanvasContext2D& canvas) {
        canvas.draw_svg(icon, {0.0f, 0.0f, 16.0f, 16.0f});
    }}};
    ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    check_quadrants(renderer);
}

void cache_reuses_parsed_resources_and_failures() {
    CountingProvider provider;
    ui::SvgCache cache{provider};

    const auto first = cache.load("good");
    NUI_CHECK(first);
    NUI_CHECK(first.error == ui::SvgLoadError::None);
    NUI_CHECK(provider.calls == 1);
    NUI_CHECK(cache.size() == 1);

    const auto second = cache.load("good");
    NUI_CHECK(second);
    NUI_CHECK(second.icon == first.icon);
    NUI_CHECK(provider.calls == 1);
    NUI_CHECK(cache.size() == 1);

    const auto missing = cache.load("missing");
    NUI_CHECK(!missing);
    NUI_CHECK(missing.error == ui::SvgLoadError::NotFound);
    NUI_CHECK(provider.calls == 2);
    NUI_CHECK(cache.load("missing").error == ui::SvgLoadError::NotFound);
    NUI_CHECK(provider.calls == 2);

    const auto malformed = cache.load("bad");
    NUI_CHECK(!malformed);
    NUI_CHECK(malformed.error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 3);
    NUI_CHECK(cache.load("bad").error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 3);
    NUI_CHECK(cache.size() == 3);

    cache.clear();
    NUI_CHECK(cache.size() == 0);
    NUI_CHECK(cache.load("good"));
    NUI_CHECK(provider.calls == 4);
}

void suite() {
    parse_and_render_at_logical_size();
    default_fit_preserves_intrinsic_aspect_ratio();
    viewbox_only_icons_use_their_viewbox_as_intrinsic_size();
    cache_reuses_parsed_resources_and_failures();
}

} // namespace

int main() { return test::run("svg", &suite); }
