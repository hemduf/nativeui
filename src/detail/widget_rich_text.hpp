#pragma once
#include "shaped_text_backend.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <nativeui/rich_text.hpp>
#include <string>
#include <vector>
namespace ui::detail {
struct RichTextGlyphRun {
  SkFont font;
  std::vector<SkGlyphID> glyphs;
  std::vector<SkPoint> positions;
  std::vector<std::uint32_t> clusters;
  std::size_t paragraph{}, span{}, line{};
  std::uint8_t bidi_level{};
  float advance{};
  Rect bounds{};
  float baseline{}, underline_position{}, underline_thickness{},
      strike_position{}, strike_thickness{};
};
struct RichTextCluster {
  std::size_t paragraph{}, begin{}, end{}, span{}, line{};
  Rect bounds{};
};
struct RichTextLayout {
  std::vector<std::string> paragraphs;
  std::string repaired_text;
  std::vector<std::string> span_text;
  std::vector<TextStyle> span_styles;
  std::vector<RichTextGlyphRun> runs;
  std::vector<RichTextCluster> clusters;
  std::vector<std::vector<Rect>> span_rectangles;
  Size preferred{};
  float first_baseline{}, minimum_width{};
  std::size_t shaped_paragraphs{}, font_runs{};
};
enum class RichTextReflowPhase { None, FontSnapshot, Shape, Publish };
[[nodiscard]] std::shared_ptr<const RichTextLayout>
prepare_rich_text_layout(const std::vector<RichTextSpan> &, const TextStyle &,
                         float width, bool wrap,
                         RichTextReflowPhase fault = RichTextReflowPhase::None);
struct RichTextTestAccess {
  [[nodiscard]] static std::shared_ptr<const RichTextLayout>
  layout(const Component &);
  static void fail_next_reflow(Component &, RichTextReflowPhase);
};
} // namespace ui::detail
