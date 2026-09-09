#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

namespace text {

/// Borrow a complete UTF-8 prefix of at most max_bytes. Returns nullopt when
/// malformed text is encountered before the limit; the remaining suffix is
/// not inspected. Uses the renderer's decoder, without allocation or font I/O.
[[nodiscard]] std::optional<std::string_view> utf8_prefix(
    std::string_view value, std::size_t max_bytes = std::string_view::npos) noexcept;

} // namespace text

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
    /// Register an in-memory font under a platform-neutral family alias.
    ///
    /// Embedded registrations are intentionally process-shared within one
    /// NativeUI image and immutable by alias. Repeating an alias with identical
    /// bytes succeeds idempotently. Reusing the alias with different bytes
    /// fails and never replaces the font already visible to other UI/plugin
    /// instances. The caller may release the source buffer after this call.
    ///
    /// Plug-in consumers should therefore use collision-resistant aliases
    /// (normally namespaced by vendor/product) unless process-wide sharing of a
    /// font resource is explicitly intended.
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
    /// Measure UTF-8 text. Malformed bytes are replaced with U+FFFD, using
    /// exactly the same repaired text as Painter::text.
    [[nodiscard]] static TextMetrics measure(std::string_view text, const TextStyle& style);
    [[nodiscard]] static TextMetrics measure(std::string_view text, float size) {
        TextStyle style{};
        style.size = size;
        return measure(text, style);
    }
};

} // namespace ui
