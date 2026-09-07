#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

enum class TextAlign { Left, Center, Right };
enum class FontWeight { Regular = 400, Bold = 700 };
enum class FontSlant { Upright, Italic, Oblique };

struct TextStyle {
    float size{14.0f};
    Color color{colors::text};
    TextAlign align{TextAlign::Left};
    FontWeight weight{FontWeight::Regular};
    FontSlant slant{FontSlant::Upright};
    std::string family;
    std::vector<std::string> fallback_families;
};

struct TextMetrics {
    float width{};
    float height{};
    float ascent{};
    float descent{};
    float leading{};
};

struct FontMatch {
    std::string family;
    bool embedded{};
    bool glyph_available{};

    [[nodiscard]] explicit operator bool() const noexcept { return !family.empty(); }
};

class FontManager {
public:
    /// Register or replace an in-memory font under a platform-neutral family
    /// alias. The byte buffer is copied/retained by the Skia typeface and may be
    /// released by the caller after this function returns.
    [[nodiscard]] static bool register_embedded_font(
        std::string_view family_alias,
        std::span<const std::byte> data);

    /// Returns true when the alias refers to an embedded face or the platform
    /// font manager can resolve the named system family.
    [[nodiscard]] static bool has_family(std::string_view family);

    /// Resolve the face NativeUI would use for one Unicode scalar value. This
    /// exposes only diagnostic metadata; Skia/platform font objects remain
    /// private to the implementation.
    [[nodiscard]] static FontMatch match(const TextStyle& style, char32_t codepoint = U'\0');
};

class TextService {
public:
    [[nodiscard]] static TextMetrics measure(std::string_view text, const TextStyle& style);
    [[nodiscard]] static TextMetrics measure(std::string_view text, float size) {
        TextStyle style{};
        style.size = size;
        return measure(text, style);
    }
};

} // namespace ui
