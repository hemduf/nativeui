#include "example_support.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kDemoSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2">
  <rect x="0" y="0" width="1" height="1" fill="#ff0000"/>
  <rect x="1" y="0" width="1" height="1" fill="#00ff00"/>
  <rect x="0" y="1" width="1" height="1" fill="#0000ff"/>
  <rect x="1" y="1" width="1" height="1" fill="#ffffff"/>
</svg>)svg";

std::vector<std::byte> encoded_svg() {
    const auto* begin = reinterpret_cast<const std::byte*>(kDemoSvg.data());
    return {begin, begin + kDemoSvg.size()};
}

class EmbeddedProvider final : public ui::ResourceProvider {
public:
    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) override {
        ++calls;
        if (resource_id != "demo/quadrants.svg") return std::nullopt;
        return encoded_svg();
    }

    int calls{};
};

bool quadrants_rendered(const ui::HeadlessRenderer& renderer) {
    // The 2x2 square SVG is drawn into a 24x12 destination using the public
    // centered contain-fit contract, so the rendered content occupies x=6..18.
    const auto red = renderer.pixel(8, 2);
    const auto green = renderer.pixel(15, 2);
    const auto blue = renderer.pixel(8, 9);
    const auto white = renderer.pixel(15, 9);
    return red.r > 200 && red.g < 40 && red.b < 40 &&
           green.g > 200 && green.r < 40 && green.b < 40 &&
           blue.b > 200 && blue.r < 40 && blue.g < 40 &&
           white.r > 200 && white.g > 200 && white.b > 200;
}

} // namespace

int main(int argc, char** argv) {
    EmbeddedProvider provider;
    ui::SvgCache cache{provider};

    const auto first = cache.load("demo/quadrants.svg");
    if (!first) return example::fail("embedded SVG parse failed");
    const auto second = cache.load("demo/quadrants.svg");
    if (!second || second.icon != first.icon || provider.calls != 1) {
        return example::fail("SVG cache did not reuse the parsed DOM");
    }

    const auto icon = first.icon;
    if (example::self_test_requested(argc, argv)) {
        ui::UI tree{ui::Canvas{24.0f, 12.0f, [icon](ui::CanvasContext2D& g) {
            g.draw_svg(icon, {0.0f, 0.0f, 24.0f, 12.0f});
        }}};
        ui::HeadlessRenderer renderer{{24.0f, 12.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless SVG render failed");
        if (!quadrants_rendered(renderer)) {
            return example::fail("centered contain-fit SVG rendering changed quadrant colors");
        }
        return 0;
    }

    auto tree = std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T023 / SVG ICON RESOURCES"},
            ui::Canvas{520.0f, 270.0f, [icon](ui::CanvasContext2D& g) {
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                g.draw_svg(icon, {24.0f, 30.0f, 80.0f, 80.0f});
                g.draw_svg(icon, {150.0f, 30.0f, 150.0f, 80.0f});
                g.draw_svg(icon, {350.0f, 30.0f, 80.0f, 160.0f});
                g.text({24.0f, 225.0f}, "Parsed once, cached, aspect-preserving render at arbitrary logical sizes",
                       11.0f, ui::colors::textMuted);
            }}
        }.padding(20.0f).gap(12.0f));
    return example::run_window(*tree, "NativeUI T023 - SVG Icons", {600.0f, 440.0f});
}
