#include "test_support.hpp"
#include "golden/golden.hpp"

#include <nativeui/image.hpp>
#include <nativeui/svg.hpp>

#include <cstddef>
#include <limits>
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

constexpr std::string_view kRedSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2">
  <path d="M0 0H2V2H0Z" fill="#ff0000"/>
</svg>)svg";

constexpr std::string_view kBlueSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2">
  <path d="M0 0H2V2H0Z" fill="#0000ff"/>
</svg>)svg";

constexpr std::string_view kTransformGradientSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="10" viewBox="0 0 20 10">
  <defs>
    <linearGradient id="paint" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#ff0000"/>
      <stop offset="100%" stop-color="#0000ff"/>
    </linearGradient>
  </defs>
  <g transform="translate(2 1)">
    <path d="M0 0H16V8H0Z" fill="url(#paint)"/>
  </g>
</svg>)svg";

constexpr std::string_view kMalformedSvg = "<svg><path d=\"M0 0\"></svg";

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
        if (resource_id == "malformed") return byte_vector(kMalformedSvg);
        return std::nullopt;
    }

    int calls{};
};

class FixedProvider final : public ui::ResourceProvider {
public:
    explicit FixedProvider(std::string_view svg)
        : svg_(svg) {}

    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) override {
        ++calls;
        if (resource_id != "same") return std::nullopt;
        return byte_vector(svg_);
    }

    std::string_view svg_;
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

void path_icon_matches_golden_pixels() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kRedSvg));
    NUI_CHECK(icon.valid());

    ui::UI tree{ui::Canvas{8.0f, 8.0f, [icon](ui::CanvasContext2D& canvas) {
        canvas.draw_svg(icon, {0.0f, 0.0f, 8.0f, 8.0f});
    }}};
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    test::golden::Image expected{8, 8, {}};
    expected.rgb.resize(8U * 8U * 3U);
    for (std::size_t offset = 0; offset < expected.rgb.size(); offset += 3U) {
        expected.rgb[offset + 0U] = 255U;
        expected.rgb[offset + 1U] = 0U;
        expected.rgb[offset + 2U] = 0U;
    }

    test::golden::CompareOptions options;
    options.channel_tolerance = 1;
    options.compare_regions = {test::golden::Region{2, 2, 4, 4}};
    const auto result = test::golden::compare(
        expected, test::golden::from_renderer(renderer), options);
    NUI_CHECK(result.matched);
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
    // default. With independent X/Y scaling these margin pixels become SVG
    // colors instead of remaining background.
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

void path_transform_and_gradient_render_headlessly() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kTransformGradientSvg));
    NUI_CHECK(icon.valid());

    ui::UI tree{ui::Canvas{20.0f, 10.0f, [icon](ui::CanvasContext2D& canvas) {
        canvas.fill_rect({0.0f, 0.0f, 20.0f, 10.0f}, ui::colors::background);
        canvas.draw_svg(icon, {0.0f, 0.0f, 20.0f, 10.0f});
    }}};
    ui::HeadlessRenderer renderer{{20.0f, 10.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto outside = renderer.pixel(0, 5);
    NUI_CHECK(outside.r < 40 && outside.g < 40 && outside.b < 40);

    const auto left = renderer.pixel(4, 5);
    const auto right = renderer.pixel(15, 5);
    NUI_CHECK(static_cast<int>(left.r) > static_cast<int>(left.b) + 80);
    NUI_CHECK(static_cast<int>(right.b) > static_cast<int>(right.r) + 80);
}

void malformed_and_empty_svg_fail_deterministically() {
    NUI_CHECK(!ui::SvgIcon::parse({}));
    NUI_CHECK(!ui::SvgIcon::parse(as_bytes(kMalformedSvg)));
}

void invalid_destination_rectangles_are_safe_noops() {
    const auto icon = ui::SvgIcon::parse(as_bytes(kRedSvg));
    NUI_CHECK(icon.valid());

    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ui::UI tree{ui::Canvas{8.0f, 8.0f, [icon, infinity, nan](ui::CanvasContext2D& canvas) {
        canvas.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
        canvas.draw_svg(icon, {0.0f, 0.0f, 0.0f, 4.0f});
        canvas.draw_svg(icon, {0.0f, 0.0f, -1.0f, 4.0f});
        canvas.draw_svg(icon, {infinity, 0.0f, 4.0f, 4.0f});
        canvas.draw_svg(icon, {0.0f, nan, 4.0f, 4.0f});
    }}};
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto untouched = renderer.pixel(3, 3);
    NUI_CHECK(untouched.g > 220 && untouched.r < 30 && untouched.b < 30);
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

    const auto malformed = cache.load("malformed");
    NUI_CHECK(!malformed);
    NUI_CHECK(malformed.error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 3);
    NUI_CHECK(cache.load("malformed").error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 3);

    const auto empty = cache.load("bad");
    NUI_CHECK(!empty);
    NUI_CHECK(empty.error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 4);
    NUI_CHECK(cache.load("bad").error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 4);
    NUI_CHECK(cache.size() == 4);

    cache.clear();
    NUI_CHECK(cache.size() == 0);
    NUI_CHECK(cache.load("good"));
    NUI_CHECK(provider.calls == 5);
    NUI_CHECK(cache.load("bad").error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 6);
    NUI_CHECK(cache.load("bad").error == ui::SvgLoadError::ParseFailed);
    NUI_CHECK(provider.calls == 6);
}

void independent_caches_can_reuse_the_same_resource_id() {
    FixedProvider red_provider{kRedSvg};
    FixedProvider blue_provider{kBlueSvg};
    ui::SvgCache red_cache{red_provider};
    ui::SvgCache blue_cache{blue_provider};

    const auto red = red_cache.load("same");
    const auto blue = blue_cache.load("same");
    NUI_CHECK(red && blue);
    NUI_CHECK(red_provider.calls == 1);
    NUI_CHECK(blue_provider.calls == 1);
    NUI_CHECK(red_cache.size() == 1);
    NUI_CHECK(blue_cache.size() == 1);

    ui::UI tree{ui::Canvas{8.0f, 4.0f, [red = red.icon, blue = blue.icon](ui::CanvasContext2D& canvas) {
        canvas.draw_svg(red, {0.0f, 0.0f, 4.0f, 4.0f});
        canvas.draw_svg(blue, {4.0f, 0.0f, 4.0f, 4.0f});
    }}};
    ui::HeadlessRenderer renderer{{8.0f, 4.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto red_pixel = renderer.pixel(1, 1);
    const auto blue_pixel = renderer.pixel(6, 1);
    NUI_CHECK(red_pixel.r > 220 && red_pixel.g < 30 && red_pixel.b < 30);
    NUI_CHECK(blue_pixel.b > 220 && blue_pixel.r < 30 && blue_pixel.g < 30);
}

void suite() {
    parse_and_render_at_logical_size();
    path_icon_matches_golden_pixels();
    default_fit_preserves_intrinsic_aspect_ratio();
    path_transform_and_gradient_render_headlessly();
    malformed_and_empty_svg_fail_deterministically();
    invalid_destination_rectangles_are_safe_noops();
    viewbox_only_icons_use_their_viewbox_as_intrinsic_size();
    cache_reuses_parsed_resources_and_failures();
    independent_caches_can_reuse_the_same_resource_id();
}

} // namespace

int main() { return test::run("svg", &suite); }
