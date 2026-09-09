#include "test_support.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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

bool renders_red_ink(std::string text, const ui::TextStyle& style) {
    ui::UI label{ui::Label{std::move(text)}.style(style)};
    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};
    if (!renderer.render(label)) return false;

    const auto& pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if (pixels[i] > 100 && pixels[i] > pixels[i + 1] && pixels[i] > pixels[i + 2]) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint8_t> render_canvas_text(std::string text, const ui::TextStyle& style) {
    ui::UI canvas{ui::Canvas{320.0f, 48.0f, [text = std::move(text), style](ui::CanvasContext2D& g) {
        g.text({12.0f, 24.0f}, text, style);
    }}};
    ui::HeadlessRenderer renderer{{320.0f, 48.0f}, 1.0f};
    NUI_CHECK(renderer.render(canvas));
    return renderer.rgba_pixels();
}

void suite() {
    ui::TextStyle system_style{};
    // A JPEG header used to reach Skia unchanged even though the fallback
    // decoder selected U+FFFD, aborting in macOS textToGlyphs/measureText.
    const std::string jpeg_header{"\xFF\xD8\xFF\xE1", 4};
    const std::string repaired_header =
        "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD";
    NUI_CHECK_NEAR(ui::TextService::measure(jpeg_header, system_style).width,
                   ui::TextService::measure(repaired_header, system_style).width, 0.01f);

    const std::string replacement{"\xEF\xBF\xBD"};
    const std::array malformed{
        std::pair{jpeg_header, repaired_header},
        std::pair{std::string{"A\x80"}, "A" + replacement},
        std::pair{std::string{"\xC0\xAF"}, replacement + replacement},
        std::pair{std::string{"\xED\xA0\x80"}, replacement + replacement + replacement},
        std::pair{std::string{"\xF4\x90\x80\x80"}, repaired_header},
        std::pair{std::string{"A\xE2\x82"}, "A" + replacement + replacement},
        std::pair{std::string{"\xF0\x9F\x92"}, replacement + replacement + replacement},
        std::pair{std::string{"\xC3\xA9\xFF" "B"}, std::string{"\xC3\xA9"} + replacement + "B"},
    };
    for (const auto& [input, expected] : malformed) {
        NUI_CHECK(!ui::text::utf8_prefix(input));
        NUI_CHECK(ui::text::utf8_prefix(expected) == expected);
        const auto actual_metrics = ui::TextService::measure(input, system_style);
        const auto expected_metrics = ui::TextService::measure(expected, system_style);
        NUI_CHECK(std::isfinite(actual_metrics.width));
        NUI_CHECK_NEAR(actual_metrics.width, expected_metrics.width, 0.01f);
        // Same-platform pixel equivalence proves painting uses the repaired
        // bytes too, not only the safe measurement/fallback scalar values.
        NUI_CHECK(render_canvas_text(input, system_style) == render_canvas_text(expected, system_style));
    }

    const std::string valid_text{"A\xC3\xA9\xF0\x9F\x9A\x80" "B"};
    const std::array<std::size_t, 9> boundaries{0, 1, 1, 3, 3, 3, 3, 7, 8};
    for (std::size_t limit = 0; limit < boundaries.size(); ++limit) {
        const auto prefix = ui::text::utf8_prefix(valid_text, limit);
        NUI_CHECK(prefix && prefix->size() == boundaries[limit]);
        NUI_CHECK(prefix->data() == valid_text.data());
    }
    NUI_CHECK(ui::text::utf8_prefix("A\xFF", 1) == "A");
    NUI_CHECK(!ui::text::utf8_prefix("A\xFF", 2));
    // UTF-8 validity is not a file-type/control-character policy.
    NUI_CHECK(ui::text::utf8_prefix(std::string_view{"a\0b", 3}).has_value());
    NUI_CHECK(ui::text::utf8_prefix("").has_value());
    for (unsigned int byte = 0; byte < 256; ++byte) {
        const char character = static_cast<char>(byte);
        const auto prefix = ui::text::utf8_prefix(std::string_view{&character, 1});
        NUI_CHECK(prefix.has_value() == (byte < 0x80));
    }

    const auto repaired_layout = ui::detail::resolve_text_layout(jpeg_header, system_style);
    const auto valid_layout = ui::detail::resolve_text_layout(replacement, system_style);
    NUI_CHECK(valid_layout.repaired_text.empty());
    NUI_CHECK(valid_layout.text_bytes(replacement).data() == replacement.data());
    NUI_CHECK(repaired_layout.text_bytes(jpeg_header) == repaired_header);
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

    // The process-shared registry is immutable by alias. Independent UI/plugin
    // instances may repeat the same registration, but a second consumer must
    // never be able to replace an already-published alias with different data.
    NUI_CHECK(ui::FontManager::register_embedded_font("NativeUI Test Latin", latin));
    NUI_CHECK(!ui::FontManager::register_embedded_font("NativeUI Test Latin", fallback));

    ui::TextStyle latin_only{};
    latin_only.family = "NativeUI Test Latin";
    const auto still_latin = ui::FontManager::match(latin_only, U'A');
    NUI_CHECK(still_latin);
    NUI_CHECK(still_latin.embedded);
    NUI_CHECK(still_latin.glyph_available);
    NUI_CHECK(still_latin.family == "NativeUI Test Latin");

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

    // Painting must use the same resolved runs as measurement. Verify each
    // deterministic fixture glyph independently so a visible primary glyph
    // cannot hide a broken fallback draw path.
    NUI_CHECK(renders_red_ink("A", style));
    NUI_CHECK(renders_red_ink(std::string{omega}, style));
    NUI_CHECK(renders_red_ink(std::string{cjk}, style));
    NUI_CHECK(renders_red_ink(mixed, style));
}

} // namespace

int main() { return test::run("font-manager", &suite); }
