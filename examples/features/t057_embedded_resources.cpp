#include "example_support.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace {

constexpr std::string_view kIconSvg =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2"><rect x="0" y="0" width="1" height="2" fill="#4aa3ff"/><rect x="1" y="0" width="1" height="2" fill="#ffffff"/></svg>)svg";
constexpr std::array<std::byte, 4> kPayload{
    std::byte{0x4e}, std::byte{0x55}, std::byte{0x49}, std::byte{0x00}};

std::span<const std::byte> icon_bytes() noexcept {
    return {reinterpret_cast<const std::byte*>(kIconSvg.data()), kIconSvg.size()};
}

std::span<const ui::EmbeddedResourceEntry> embedded_table() noexcept {
    static const std::array<ui::EmbeddedResourceEntry, 2> entries{{
        {"demo/icon.svg", icon_bytes()},
        {"demo/payload.bin", kPayload},
    }};
    return entries;
}

bool direct_contract(const ui::ResourceManager& resources) {
    const auto payload = resources.find("demo/payload.bin");
    return resources.valid() && resources.resources().size() == 2U && payload &&
           payload->bytes.data() == kPayload.data() && payload->bytes.size() == kPayload.size() &&
           payload->bytes[3] == std::byte{0x00} && !resources.contains("demo/missing.bin");
}

} // namespace

int main(int argc, char** argv) {
    const ui::ResourceManager resources{embedded_table()};
    if (!direct_contract(resources)) return example::fail("ResourceManager direct lookup contract failed");

    ui::ResourceManagerProvider provider{resources};
    const auto owned = provider.load("demo/payload.bin");
    if (!owned || owned->size() != kPayload.size() || owned->data() == kPayload.data()) {
        return example::fail("ResourceManagerProvider did not create an owned payload copy");
    }

    ui::SvgCache svg_cache{provider};
    const auto loaded_icon = svg_cache.load("demo/icon.svg");
    if (!loaded_icon) return example::fail("embedded SVG adapter load failed");
    const auto icon = loaded_icon.icon;

    if (example::self_test_requested(argc, argv)) {
        ui::UI tree{ui::Canvas{32.0f, 16.0f, [icon](ui::CanvasContext2D& canvas) {
            canvas.draw_svg(icon, {0.0f, 0.0f, 32.0f, 16.0f});
        }}};
        ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("embedded resource SVG render failed");
        // The 2x2 square is contain-fitted into the 32x16 destination, so its
        // rendered content occupies x=8..24 rather than the full rectangle.
        const auto left = renderer.pixel(10, 8);
        const auto right = renderer.pixel(22, 8);
        if (!(left.b > left.r && left.b > left.g &&
              right.r > 200 && right.g > 200 && right.b > 200)) {
            return example::fail("embedded resource SVG pixels changed");
        }
        return 0;
    }

    auto tree = std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T057 / EMBEDDED RESOURCE MANAGER"},
            ui::TextLabel{"Direct lookup is borrowed, immutable and allocation-free."},
            ui::TextLabel{"ResourceManagerProvider is the explicit allocating compatibility seam."},
            ui::Canvas{260.0f, 130.0f, [icon](ui::CanvasContext2D& canvas) {
                canvas.fill_rounded_rect({0.0f, 0.0f, canvas.width(), canvas.height()},
                                         12.0f, ui::colors::panel);
                canvas.draw_svg(icon, {34.0f, 24.0f, 192.0f, 82.0f});
            }}
        }.padding(20.0f).gap(12.0f));
    return example::run_window(*tree, "NativeUI T057 - Embedded Resources", {620.0f, 420.0f});
}
