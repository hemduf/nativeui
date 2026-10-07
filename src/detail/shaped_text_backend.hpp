#pragma once

// Private owned-font and final-glyph bridge for the retained text layout.
// Never install this header or include it from nativeui/rich_text.hpp.
#include <nativeui/geometry.hpp>
#include <nativeui/text.hpp>

#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace ui {
class Painter;
namespace detail {

// Defined only in skia_core.cpp: one immutable EmbeddedFaces snapshot plus
// a strong reference to the existing platform_font_manager() result.
struct ShapingFonts;
using ShapingFontSnapshot = std::shared_ptr<const ShapingFonts>;

// Fallible preparation; no additional global manager or registry.
[[nodiscard]] ShapingFontSnapshot capture_shaping_fonts();

// Return an owning ref to that exact captured manager for the HB factory.
// FontRunIterator still supplies fonts resolved through the NativeUI chain.
[[nodiscard]] sk_sp<SkFontMgr> shaping_font_manager(const ShapingFonts&) noexcept;

struct ShapingFontMatch final {
  SkFont font;
  bool complete_coverage{};
};

// required_scalars belongs to ONE complete grapheme, after filtering shaping
// controls/default-ignorable selectors from glyph-coverage requirements.
// Empty coverage selects the U'\0' metrics face, as current TextService does.
// Try the existing NativeUI family/fallback chain for complete coverage;
// absent a covering face, retain resolve_face's ordinary .notdef fallback and
// return complete_coverage=false. nullopt means no usable typeface exists.
// Returned SkFont owns its typeface and uses the current make_font policy.
[[nodiscard]] std::optional<ShapingFontMatch> resolve_shaping_font(
    const ShapingFonts&, const TextStyle&, std::span<const char32_t> required_scalars);

struct ShapedTextPaintAccess final {
  // Synchronous borrowed view only: never retain Painter, arrays or UTF-8.
  // positions are final baseline positions relative to the paragraph origin.
  // clusters are offsets into the full repaired paragraph UTF-8 buffer, in
  // glyph order (including RTL order); all arrays have the same glyph count.
  // Shape/align/line placement is already committed by the caller's Layout.
  static void draw_glyph_run(
      Painter&, Point paragraph_origin, const SkFont&,
      std::span<const SkGlyphID> glyphs,
      std::span<const SkPoint> positions,
      std::span<const std::uint32_t> clusters,
      std::string_view repaired_paragraph_utf8, Color);
};

} // namespace detail
} // namespace ui
