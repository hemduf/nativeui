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

/// Borrow the largest complete UTF-8 prefix that fits within `max_bytes`.
///
/// `value` is never copied and the returned string_view aliases its storage.
/// `max_bytes` is a byte limit, not a scalar/grapheme count. If the limit falls
/// inside a multi-byte scalar, that scalar is omitted. The default `npos`
/// examines the complete input; a zero limit returns an engaged empty view.
///
/// Returns std::nullopt when malformed UTF-8 is encountered before the selected
/// boundary. Bytes after the limit are deliberately not inspected.
///
/// This helper allocates nothing, performs no font I/O/callbacks and is noexcept.
/// The returned view must not outlive `value`.
[[nodiscard]] std::optional<std::string_view> utf8_prefix(
    std::string_view value, std::size_t max_bytes = std::string_view::npos) noexcept;

} // namespace text

/// Horizontal anchor used by Painter text placement.
///
/// Left anchors the run's left edge at x, Center centers it on x and Right
/// anchors the right edge on x. Alignment changes placement, not measurement.
enum class TextAlign { Left, Center, Right };

/// Requested font weight for face matching; values follow conventional 400/700.
enum class FontWeight { Regular = 400, Bold = 700 };

/// Requested upright, italic or oblique face slant.
enum class FontSlant { Upright, Italic, Oblique };

/// Owned backend-neutral text presentation request.
///
/// Geometry uses NativeUI logical UI units. Family names are owned; no
/// renderer-native font handle is retained. Face discovery happens when text is
/// measured or painted.
struct TextStyle {
    /// Requested font size in logical UI units. Negative values clamp to zero.
    float size{14.0f};

    /// Paint color; it does not affect measurement.
    Color color{colors::text};

    /// Horizontal placement anchor; it does not affect measurement.
    TextAlign align{TextAlign::Left};

    /// Requested regular/bold face weight.
    FontWeight weight{FontWeight::Regular};

    /// Requested face slant.
    FontSlant slant{FontSlant::Upright};

    /// Preferred owned family name; empty skips directly to fallback resolution.
    std::string family;

    /// Ordered owned fallback family chain; empty entries are skipped.
    std::vector<std::string> fallback_families;
};

/// Logical-unit metrics for one measured UTF-8 sequence.
///
/// `width` sums resolved run widths. `ascent` and `descent` retain the
/// backend's signed baseline-relative convention; `leading` is the maximum
/// resolved-run leading and `height` is max(0, descent - ascent + leading).
///
/// If no usable face resolves, fields remain zero. Empty text can still report
/// vertical metrics when a face is available.
struct TextMetrics {
    /// Horizontal advance in logical UI units.
    float width{};
    /// Total line height in logical UI units.
    float height{};
    /// Signed baseline-relative ascent, normally negative.
    float ascent{};
    /// Signed baseline-relative descent, normally positive.
    float descent{};
    /// Additional line leading in logical UI units.
    float leading{};
};

/// Diagnostic description of the face selected for a style/code point.
///
/// `family` is owned and no renderer-native typeface escapes. `embedded`
/// identifies a NativeUI embedded face. `glyph_available` reports actual
/// coverage and can be false when only a .notdef face is available.
struct FontMatch {
    /// Owned resolved family/alias; empty means no face resolved.
    std::string family;
    /// Whether the selected face came from NativeUI's embedded registry.
    bool embedded{};
    /// Whether the selected face contains the requested scalar.
    bool glyph_available{};

    /// Presence test for a resolved family, not a glyph-coverage test.
    [[nodiscard]] explicit operator bool() const noexcept { return !family.empty(); }
};

/// Process-wide font registration and diagnostic matching services.
///
/// Embedded faces are shared by NativeUI instances in the same process. Registry
/// access is synchronized and operations may allocate or consult platform font
/// services. No application callbacks are invoked; these are not audio-RT APIs.
class FontManager {
public:
    /// Register an in-memory font under a platform-neutral family alias.
    ///
    /// `family_alias` and `data` are borrowed only for this call; successful
    /// registration retains its own font data. Empty alias/payload, unparseable
    /// data, or reuse of an alias with different bytes returns false.
    ///
    /// Registrations are process-shared and immutable by alias. Repeating an
    /// alias with byte-identical data succeeds idempotently and never replaces
    /// the face already visible to other UI/plugin instances.
    ///
    /// The operation may allocate, parse font bytes, synchronize the registry
    /// and initialize platform font services. It invokes no application
    /// callbacks but must not run in an audio/DSP real-time callback.
    ///
    /// Plug-in consumers should use collision-resistant aliases (normally
    /// namespaced by vendor/product) unless process-wide sharing is intended.
    [[nodiscard]] static bool register_embedded_font(
        std::string_view family_alias,
        std::span<const std::byte> data);

    /// Test whether an exact family name is currently resolvable.
    ///
    /// Embedded aliases are checked first and then the platform font manager.
    /// Empty names return false. The input is borrowed only for this call.
    /// Lookup may allocate/synchronize or initialize platform font services and
    /// is not audio-real-time safe.
    [[nodiscard]] static bool has_family(std::string_view family);

    /// Resolve the face NativeUI would select for one Unicode scalar value.
    ///
    /// Resolution tries `style.family`, then non-empty
    /// `style.fallback_families` in order, then platform Unicode fallback. On
    /// WebAssembly, registered embedded faces provide the final browser fallback.
    /// If no face covers the scalar, a face may still be returned for .notdef;
    /// inspect `glyph_available` to distinguish that case.
    ///
    /// U'\\0' requests a representative face without a coverage requirement.
    /// The returned FontMatch owns its family string. Matching may allocate or
    /// consult synchronized/platform font services and is not RT-safe.
    [[nodiscard]] static FontMatch match(const TextStyle& style, char32_t codepoint = U'\0');
};

/// Text measurement using the same UTF-8 repair and face-resolution path as
/// Painter text drawing.
///
/// Calls may allocate for repair/run storage/family lookup and may synchronize
/// through font/platform services. They invoke no application callbacks, but
/// belong to the UI/preparation domain rather than an audio real-time callback.
class TextService {
public:
    /// Measure UTF-8 text with an explicit style.
    ///
    /// `text` is borrowed only for this call. Malformed byte sequences are
    /// replaced with U+FFFD before face resolution, exactly as in Painter::text.
    /// Font size and returned dimensions are logical UI units; color and
    /// horizontal alignment do not alter metrics.
    ///
    /// The result is owned. Allocation failures are not converted into sentinel
    /// metrics and may propagate as exceptions.
    [[nodiscard]] static TextMetrics measure(std::string_view text, const TextStyle& style);
    /// Convenience measurement using a default TextStyle with only `size`
    /// replaced; default family/fallback/weight/slant behavior is preserved.
    [[nodiscard]] static TextMetrics measure(std::string_view text, float size) {
        TextStyle style{};
        style.size = size;
        return measure(text, style);
    }
};

} // namespace ui
