#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifndef NATIVEUI_TEST_FONT_DIR
#  error "NATIVEUI_TEST_FONT_DIR must point at tests/resources"
#endif

namespace {

std::vector<std::byte> read_font(std::string_view filename) {
    const std::string path = std::string{NATIVEUI_TEST_FONT_DIR} + "/" + std::string{filename};
    std::ifstream stream(path, std::ios::binary);
    NUI_CHECK(stream.good());
    std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes;
    bytes.reserve(chars.size());
    for (const char value : chars) {
        bytes.push_back(std::byte{static_cast<unsigned char>(value)});
    }
    return bytes;
}

void suite() {
    ui::TextStyle system_style{};
    const auto system_match = ui::FontManager::match(system_style, U'A');
    NUI_CHECK(system_match);
    NUI_CHECK(!system_match.family.empty());
    NUI_CHECK(system_match.glyph_available);
    NUI_CHECK(!system_match.embedded);

    const auto latin = read_font("NativeUITestLatin.ttf");
    const auto fallback = read_font("NativeUITestFallback.ttf");
    NUI_CHECK(!latin.empty());
    NUI_CHECK(!fallback.empty());

    const std::array<std::byte, 4> invalid{};
    NUI_CHECK(!ui::FontManager::register_embedded_font("NativeUI Invalid", invalid));

    NUI_CHECK(ui::FontManager::register_embedded_font("NativeUI Test Latin", latin));
    NUI_CHECK(ui::FontManager::register_embedded_font("NativeUI Test Fallback", fallback));
    NUI_CHECK(ui::FontManager::has_family("NativeUI Test Latin"));
    NUI_CHECK(ui::FontManager::has_family("NativeUI Test Fallback"));

    ui::TextStyle style{};
    style.size = 20.0f;
    style.color = {1.0f, 0.0f, 0.0f, 1.0f};
    style.family = "NativeUI Test Latin";
    style.fallback_families = {"NativeUI Missing", "NativeUI Test Fallback"};

    const auto latin_match = ui::FontManager::match(style, U'A');
    NUI_CHECK(latin_match);
    NUI_CHECK(latin_match.embedded);
    NUI_CHECK(latin_match.glyph_available);
    NUI_CHECK(latin_match.family == "NativeUI Test Latin");

    const auto greek_match = ui::FontManager::match(style, U'\u03A9');
    NUI_CHECK(greek_match);
    NUI_CHECK(greek_match.embedded);
    NUI_CHECK(greek_match.glyph_available);
    NUI_CHECK(greek_match.family == "NativeUI Test Fallback");

    const auto cjk_match = ui::FontManager::match(style, U'\u65E5');
    NUI_CHECK(cjk_match);
    NUI_CHECK(cjk_match.embedded);
    NUI_CHECK(cjk_match.glyph_available);
    NUI_CHECK(cjk_match.family == "NativeUI Test Fallback");

    // Style requests must remain attached to the selected family instead of
    // silently escaping the explicit fallback chain.
    auto styled = style;
    styled.weight = ui::FontWeight::Bold;
    styled.slant = ui::FontSlant::Italic;
    const auto styled_match = ui::FontManager::match(styled, U'A');
    NUI_CHECK(styled_match);
    NUI_CHECK(styled_match.embedded);
    NUI_CHECK(styled_match.family == "NativeUI Test Latin");

    constexpr std::string_view omega = "\xCE\xA9";
    constexpr std::string_view cjk = "\xE6\x97\xA5";
    const auto a_metrics = ui::TextService::measure("A", style);
    const auto omega_metrics = ui::TextService::measure(omega, style);
    const auto cjk_metrics = ui::TextService::measure(cjk, style);
    NUI_CHECK(a_metrics.width > 0.0f);
    NUI_CHECK(omega_metrics.width > a_metrics.width);
    NUI_CHECK(cjk_metrics.width > omega_metrics.width);

    const std::string mixed = std::string{"A"} + std::string{omega} + std::string{cjk};
    const auto mixed_metrics = ui::TextService::measure(mixed, style);
    NUI_CHECK_NEAR(mixed_metrics.width,
                   a_metrics.width + omega_metrics.width + cjk_metrics.width,
                   0.05f);

    ui::UI label{ui::Label{mixed}.style(style)};
    ui::HeadlessRenderer renderer{{160.0f, 48.0f}, 1.0f};
    NUI_CHECK(renderer.render(label));

    bool visible_red_ink = false;
    const auto& pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if (pixels[i] > 100 && pixels[i] > pixels[i + 1] && pixels[i] > pixels[i + 2]) {
            visible_red_ink = true;
            break;
        }
    }
    NUI_CHECK(visible_red_ink);
}

} // namespace

int main() { return test::run("font-manager", &suite); }
