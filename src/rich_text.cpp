#include "detail/widget_rich_text.hpp"
#include "detail/widget_unicode.hpp"
#include "include/core/SkFontMetrics.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/rich_text.hpp>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>
namespace ui {
namespace {
void validate_color(Color color) {
  if (!std::isfinite(color.r) || !std::isfinite(color.g) ||
      !std::isfinite(color.b) || !std::isfinite(color.a))
    throw std::invalid_argument("RichText colors must be finite");
}
void validate_text_style(const TextStyle &style) {
  if (!std::isfinite(style.size) || style.size < 0)
    throw std::invalid_argument("RichText size must be finite and nonnegative");
  validate_color(style.color);
}
void validate_spans(const std::vector<RichTextSpan> &spans,
                    const TextStyle &style) {
  validate_text_style(style);
  std::set<std::string> ids;
  for (const auto &span : spans) {
    if (span.style)
      validate_text_style(*span.style);
    if (span.background)
      validate_color(*span.background);
    if (span.on_activate && (span.id.empty() || !ids.insert(span.id).second))
      throw std::invalid_argument(
          "RichText actions require unique nonempty ids");
  }
}
float finite_extent(double value) {
  if (!std::isfinite(value) || value < 0 ||
      value > std::numeric_limits<float>::max())
    throw std::length_error("RichText geometry exceeds logical range");
  return static_cast<float>(value);
}
Rect translated(Rect rect, Point origin) noexcept {
  rect.x += origin.x;
  rect.y += origin.y;
  return rect;
}
Rect rich_union(Rect first, Rect next) noexcept {
  if (first.empty())
    return next;
  if (next.empty())
    return first;
  const auto right = std::max(first.x + first.w, next.x + next.w),
             bottom = std::max(first.y + first.h, next.y + next.h);
  const auto x = std::min(first.x, next.x), y = std::min(first.y, next.y);
  return {x, y, right - x, bottom - y};
}
struct RepairPiece {
  std::size_t begin{}, end{}, span{};
};
struct RepairedSource {
  std::string text;
  std::vector<RepairPiece> pieces;
  std::vector<std::string> names;
};
RepairedSource repair_source(const std::vector<RichTextSpan> &spans) {
  std::string raw;
  std::vector<std::size_t> ends;
  ends.reserve(spans.size());
  for (const auto &span : spans) {
    raw.append(span.text);
    ends.push_back(raw.size());
  }
  if (raw.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw std::length_error("RichText exceeds the shaping byte domain");
  RepairedSource result;
  result.names.resize(spans.size());
  result.text.reserve(raw.size());
  std::size_t owner{};
  for (std::size_t offset = 0; offset < raw.size();) {
    while (owner < ends.size() && ends[owner] <= offset)
      ++owner;
    std::string_view scalar;
    for (std::size_t limit = 1; limit <= 4; ++limit) {
      const auto prefix =
          text::utf8_prefix(std::string_view{raw}.substr(offset), limit);
      if (prefix && !prefix->empty()) {
        scalar = *prefix;
        break;
      }
    }
    const auto begin = result.text.size();
    if (scalar.empty()) {
      result.text.append("\xef\xbf\xbd");
      ++offset;
    } else {
      result.text.append(scalar);
      offset += scalar.size();
    }
    result.names[owner].append(result.text, begin, result.text.size() - begin);
    if (!result.pieces.empty() && result.pieces.back().span == owner)
      result.pieces.back().end = result.text.size();
    else
      result.pieces.push_back({begin, result.text.size(), owner});
  }
  if (result.text.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw std::length_error(
        "Repaired RichText exceeds the shaping byte domain");
  return result;
}
std::size_t owner_at(const RepairedSource &source, std::size_t byte) {
  const auto found =
      std::upper_bound(source.pieces.begin(), source.pieces.end(), byte,
                       [](std::size_t offset, const RepairPiece &piece) {
                         return offset < piece.end;
                       });
  return found == source.pieces.end() ? 0 : found->span;
}
char32_t scalar_value(std::string_view scalar) noexcept {
  const auto first = static_cast<unsigned char>(scalar.front());
  if (first < 0x80)
    return first;
  char32_t value = first & (scalar.size() == 2   ? 0x1fU
                            : scalar.size() == 3 ? 0x0fU
                                                 : 0x07U);
  for (std::size_t i = 1; i < scalar.size(); ++i)
    value = (value << 6U) | (static_cast<unsigned char>(scalar[i]) & 0x3fU);
  return value;
}
bool shaping_control(char32_t scalar, SkUnicode &unicode) {
  return unicode.isControl(static_cast<SkUnichar>(scalar)) ||
         scalar == 0x00ad || scalar == 0x034f || scalar == 0x061c ||
         (scalar >= 0x180b && scalar <= 0x180f) ||
         (scalar >= 0x200b && scalar <= 0x200f) ||
         (scalar >= 0x202a && scalar <= 0x202e) ||
         (scalar >= 0x2060 && scalar <= 0x206f) ||
         (scalar >= 0xfe00 && scalar <= 0xfe0f) || scalar == 0xfeff ||
         (scalar >= 0xe0000 && scalar <= 0xe0fff);
}
std::vector<char32_t> coverage_for(std::string_view grapheme,
                                   SkUnicode &unicode) {
  std::vector<char32_t> result;
  for (std::size_t offset = 0; offset < grapheme.size();) {
    const auto scalar = text::utf8_prefix(grapheme.substr(offset), 4);
    if (!scalar || scalar->empty())
      throw std::runtime_error("RichText repaired scalar invariant failed");
    // utf8_prefix may include several short scalars. Retain exactly one scalar.
    const auto lead = static_cast<unsigned char>(grapheme[offset]);
    const std::size_t count = lead < 0x80   ? 1
                              : lead < 0xe0 ? 2
                              : lead < 0xf0 ? 3
                                            : 4;
    const auto code = scalar_value(grapheme.substr(offset, count));
    if (!shaping_control(code, unicode))
      result.push_back(code);
    offset += count;
  }
  return result;
}
struct FontChunk {
  std::size_t end{};
  SkFont font;
};
class FontIterator final : public SkShaper::FontRunIterator {
public:
  explicit FontIterator(const std::vector<FontChunk> &chunks)
      : chunks_(chunks) {}
  void consume() override {
    if (next_ >= chunks_.size())
      throw std::runtime_error("RichText font iterator exhausted");
    current_ = next_++;
  }
  std::size_t endOfCurrentRun() const override {
    return next_ ? chunks_[current_].end : 0;
  }
  bool atEnd() const override { return next_ >= chunks_.size(); }
  const SkFont &currentFont() const override { return chunks_[current_].font; }

private:
  const std::vector<FontChunk> &chunks_;
  std::size_t current_{}, next_{};
};
struct RawRun {
  SkFont font;
  std::string language;
  SkFourByteTag script{};
  std::uint8_t level{};
  std::size_t begin{}, end{};
  SkPoint advance{};
  std::vector<SkGlyphID> glyphs;
  std::vector<SkPoint> positions, offsets;
  std::vector<std::uint32_t> clusters;
};
class OwnedHandler final : public SkShaper::RunHandler {
public:
  std::vector<RawRun> runs;
  void beginLine() override {
    if (began_)
      throw std::runtime_error(
          "Unwrapped RichText returned more than one line");
    began_ = true;
  }
  void runInfo(const RunInfo &info) override {
    RawRun run;
    run.font = info.fFont;
    run.language = info.fLanguage ? info.fLanguage : "";
    run.script = info.fScript;
    run.level = info.fBidiLevel;
    run.begin = info.utf8Range.begin();
    run.end = info.utf8Range.end();
    run.advance = info.fAdvance;
    if (info.glyphCount >
        static_cast<std::size_t>(std::numeric_limits<int>::max()))
      throw std::length_error("RichText glyph count exceeds backend range");
    run.glyphs.resize(info.glyphCount);
    run.positions.resize(info.glyphCount + 1);
    run.offsets.resize(info.glyphCount);
    run.clusters.resize(info.glyphCount);
    runs.push_back(std::move(run));
  }
  void commitRunInfo() override {}
  Buffer runBuffer(const RunInfo &info) override {
    if (index_ >= runs.size() || runs[index_].glyphs.size() != info.glyphCount)
      throw std::runtime_error("RichText RunHandler run ordering failed");
    auto &run = runs[index_];
    return {run.glyphs.empty() ? &empty_glyph_ : run.glyphs.data(),
            run.positions.data(), run.offsets.data(), run.clusters.data(),
            SkPoint::Make(0, 0)};
  }
  void commitRunBuffer(const RunInfo &) override {
    auto &run = runs[index_++];
    run.positions.back() = run.advance;
  }
  void commitLine() override {
    if (index_ != runs.size())
      throw std::runtime_error("RichText shaping did not commit every run");
  }

private:
  std::size_t index_{};
  bool began_{};
  SkGlyphID empty_glyph_{};
};
struct GlyphPiece {
  std::size_t run{}, first{}, last{};
  float advance{};
};
struct Unit {
  std::size_t begin{}, end{}, span{};
  std::uint8_t level{};
  float advance{};
  bool soft_after{}, whitespace{};
  std::vector<GlyphPiece> pieces;
};
struct LineRange {
  std::size_t begin{}, end{};
  float width{};
};
void append_span_rect(std::vector<Rect> &rectangles, Rect rect) {
  if (rect.empty())
    return;
  for (auto &existing : rectangles)
    if (existing.y == rect.y && existing.h == rect.h &&
        rect.x <= existing.x + existing.w + .01f &&
        existing.x <= rect.x + rect.w + .01f) {
      existing = rich_union(existing, rect);
      return;
    }
  rectangles.push_back(rect);
}
void decoration_metrics(const SkFont &font, detail::RichTextGlyphRun &run) {
  SkFontMetrics metrics{};
  (void)font.getMetrics(&metrics);
  if (!metrics.hasUnderlinePosition(&run.underline_position))
    run.underline_position = std::max(0.f, metrics.fDescent) * .5f;
  if (!metrics.hasUnderlineThickness(&run.underline_thickness))
    run.underline_thickness =
        std::max(0.f, metrics.fDescent - metrics.fAscent) / 16.f;
  if (!metrics.hasStrikeoutPosition(&run.strike_position))
    run.strike_position =
        metrics.fXHeight > 0 ? -metrics.fXHeight * .5f : metrics.fAscent * .4f;
  if (!metrics.hasStrikeoutThickness(&run.strike_thickness))
    run.strike_thickness = run.underline_thickness;
  for (const auto v : {run.underline_position, run.underline_thickness,
                       run.strike_position, run.strike_thickness})
    if (!std::isfinite(v))
      throw std::runtime_error("RichText font decorations are not finite");
  run.underline_thickness = std::max(0.f, run.underline_thickness);
  run.strike_thickness = std::max(0.f, run.strike_thickness);
}
} // namespace
namespace detail {
std::shared_ptr<const RichTextLayout>
prepare_rich_text_layout(const std::vector<RichTextSpan> &spans,
                         const TextStyle &base, float width, bool wrap,
                         RichTextReflowPhase fault) {
  validate_spans(spans, base);
  if (std::isnan(width) || width < 0)
    throw std::invalid_argument("RichText width must be nonnegative");
  const auto source = repair_source(spans);
  auto layout = std::make_shared<RichTextLayout>();
  layout->repaired_text = source.text;
  layout->span_text = source.names;
  layout->span_rectangles.resize(spans.size());
  layout->span_styles.reserve(spans.size());
  for (const auto &span : spans)
    layout->span_styles.push_back(span.style.value_or(base));
  if (fault == RichTextReflowPhase::FontSnapshot)
    throw std::runtime_error("Injected RichText font snapshot failure");
  auto fonts = capture_shaping_fonts();
  if (!fonts)
    throw std::runtime_error("RichText font snapshot unavailable");
  auto unicode = SkUnicodes::ICU::Make();
  if (!unicode)
    throw std::runtime_error("RichText Unicode backend unavailable");
  auto shaper = SkShapers::HB::ShapeDontWrapOrReorder(
      unicode, shaping_font_manager(*fonts));
  if (!shaper)
    throw std::runtime_error("RichText shaping backend unavailable");
  std::size_t global{}, line_index{};
  double y{};
  float maximum{};
  do {
    const auto newline = source.text.find('\n', global);
    auto end = newline == std::string::npos ? source.text.size() : newline;
    if (end > global && source.text[end - 1] == '\r')
      --end;
    const std::size_t paragraph_index = layout->paragraphs.size();
    layout->paragraphs.push_back(source.text.substr(global, end - global));
    const auto &paragraph = layout->paragraphs.back();
    const auto graphemes = widget_graphemes(paragraph);
    std::vector<FontChunk> chunks;
    for (const auto grapheme : graphemes) {
      const auto owner = owner_at(source, global + grapheme.begin);
      const auto &style = layout->span_styles[owner];
      const auto coverage =
          coverage_for(std::string_view{paragraph}.substr(
                           grapheme.begin, grapheme.end - grapheme.begin),
                       *unicode);
      const auto match = resolve_shaping_font(*fonts, style, coverage);
      if (!match)
        throw std::runtime_error("RichText could not resolve a grapheme font");
      if (!chunks.empty() && chunks.back().font == match->font)
        chunks.back().end = grapheme.end;
      else
        chunks.push_back({grapheme.end, match->font});
    }
    OwnedHandler handler;
    if (!paragraph.empty()) {
      FontIterator font_iterator{chunks};
      auto bidi = SkShapers::unicode::BidiRunIterator(unicode, paragraph.data(),
                                                      paragraph.size(), 0);
      auto script =
          SkShapers::HB::ScriptRunIterator(paragraph.data(), paragraph.size());
      SkShaper::TrivialLanguageRunIterator language{"und", paragraph.size()};
      if (!bidi || !script)
        throw std::runtime_error("RichText shaping iterators unavailable");
      if (fault == RichTextReflowPhase::Shape)
        throw std::runtime_error("Injected RichText shaping failure");
      shaper->shape(paragraph.data(), paragraph.size(), font_iterator, *bidi,
                    *script, language, nullptr, 0,
                    std::numeric_limits<float>::max(), &handler);
      ++layout->shaped_paragraphs;
      layout->font_runs += chunks.size();
      if (handler.runs.empty())
        throw std::runtime_error("RichText shaping produced no runs");
    }
    std::vector<std::size_t> grapheme_starts;
    grapheme_starts.reserve(graphemes.size());
    for (const auto grapheme : graphemes)
      grapheme_starts.push_back(grapheme.begin);
    std::vector<std::size_t> boundaries{0, paragraph.size()};
    for (const auto &run : handler.runs) {
      if (run.begin > run.end || run.end > paragraph.size())
        throw std::runtime_error("RichText run range exceeds paragraph");
      for (const auto cluster : run.clusters) {
        if (cluster >= paragraph.size() ||
            !text::utf8_prefix(std::string_view{paragraph}.substr(0, cluster)))
          throw std::runtime_error(
              "RichText glyph cluster is not a repaired scalar boundary");
        if (std::binary_search(grapheme_starts.begin(), grapheme_starts.end(),
                               cluster))
          boundaries.push_back(cluster);
      }
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                     boundaries.end());
    skia_private::TArray<SkUnicode::CodeUnitFlags, true> flags;
    std::string flag_text = paragraph;
    if (!unicode->computeCodeUnitFlags(flag_text.data(),
                                       static_cast<int>(flag_text.size()),
                                       false, &flags))
      throw std::runtime_error("RichText Unicode flags unavailable");
    std::vector<SkUnicode::BidiRegion> regions;
    if (!paragraph.empty() &&
        !unicode->getBidiRegions(paragraph.data(),
                                 static_cast<int>(paragraph.size()),
                                 SkUnicode::TextDirection::kLTR, &regions))
      throw std::runtime_error("RichText bidi regions unavailable");
    std::vector<Unit> units;
    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
      Unit unit;
      unit.begin = boundaries[i];
      unit.end = boundaries[i + 1];
      unit.span = owner_at(source, global + unit.begin);
      for (const auto &region : regions)
        if (unit.begin >= region.start && unit.begin < region.end) {
          unit.level = region.level;
          break;
        }
      unit.soft_after =
          unit.end == paragraph.size() ||
          (unit.end < static_cast<std::size_t>(flags.size()) &&
           SkUnicode::hasSoftLineBreakFlag(flags[static_cast<int>(unit.end)]));
      unit.whitespace = true;
      for (std::size_t byte = unit.begin; byte < unit.end;) {
        const auto lead = static_cast<unsigned char>(paragraph[byte]);
        const std::size_t count = lead < 0x80   ? 1
                                  : lead < 0xe0 ? 2
                                  : lead < 0xf0 ? 3
                                                : 4;
        unit.whitespace =
            unit.whitespace &&
            unicode->isWhitespace(static_cast<SkUnichar>(
                scalar_value(std::string_view{paragraph}.substr(byte, count))));
        byte += count;
      }
      units.push_back(std::move(unit));
    }
    for (std::size_t r = 0; r < handler.runs.size(); ++r) {
      const auto &run = handler.runs[r];
      for (std::size_t first = 0; first < run.glyphs.size();) {
        const auto found =
            std::upper_bound(boundaries.begin(), boundaries.end(),
                             static_cast<std::size_t>(run.clusters[first]));
        const auto index =
            static_cast<std::size_t>(found - boundaries.begin() - 1);
        std::size_t last = first + 1;
        while (last < run.glyphs.size() &&
               run.clusters[last] >= units[index].begin &&
               run.clusters[last] < units[index].end)
          ++last;
        const auto advance =
            finite_extent(static_cast<double>(run.positions[last].x()) -
                          run.positions[first].x());
        units[index].pieces.push_back({r, first, last, advance});
        units[index].advance =
            finite_extent(static_cast<double>(units[index].advance) + advance);
        first = last;
      }
    }
    std::vector<LineRange> lines;
    for (std::size_t start = 0; start < units.size();) {
      std::size_t finish = start, soft = start;
      double advance{};
      while (finish < units.size()) {
        const auto next = advance + units[finish].advance;
        if (wrap && std::isfinite(width) && finish > start && next > width)
          break;
        advance = next;
        ++finish;
        if (units[finish - 1].soft_after)
          soft = finish;
      }
      if (finish < units.size() && soft > start)
        finish = soft;
      if (finish <= start)
        throw std::runtime_error("RichText wrapping made no cluster progress");
      advance = 0;
      for (auto i = start; i < finish; ++i)
        advance += units[i].advance;
      lines.push_back({start, finish, finite_extent(advance)});
      start = finish;
    }
    if (lines.empty())
      lines.push_back({0, 0, 0});
    const auto metrics_match = resolve_shaping_font(*fonts, base, {});
    if (!metrics_match)
      throw std::runtime_error("RichText metrics font unavailable");
    for (const auto line : lines) {
      float ascent{}, descent{}, leading{};
      auto add_metrics = [&](const SkFont &font) {
        SkFontMetrics metrics{};
        (void)font.getMetrics(&metrics);
        ascent =
            std::max(ascent, finite_extent(std::max(0.f, -metrics.fAscent)));
        descent =
            std::max(descent, finite_extent(std::max(0.f, metrics.fDescent)));
        leading =
            std::max(leading, finite_extent(std::max(0.f, metrics.fLeading)));
      };
      bool has_glyph_metrics{};
      for (auto i = line.begin; i < line.end; ++i)
        for (const auto piece : units[i].pieces) {
          add_metrics(handler.runs[piece.run].font);
          has_glyph_metrics = true;
        }
      if (!has_glyph_metrics)
        add_metrics(metrics_match->font);
      const float line_height = finite_extent(static_cast<double>(ascent) +
                                              descent + leading),
                  baseline = finite_extent(y + ascent);
      if (line_index == 0)
        layout->first_baseline = ascent;
      float x{};
      if (std::isfinite(width)) {
        const auto spare = std::max(0.f, width - line.width);
        x = base.align == TextAlign::Center  ? spare * .5f
            : base.align == TextAlign::Right ? spare
                                             : 0;
      }
      std::vector<SkUnicode::BidiLevel> levels;
      std::vector<int32_t> order(line.end - line.begin);
      levels.reserve(order.size());
      for (auto i = line.begin; i < line.end; ++i)
        levels.push_back(units[i].level);
      for (auto i = line.end; i > line.begin && units[i - 1].whitespace; --i)
        levels[i - 1 - line.begin] = 0;
      if (!levels.empty())
        unicode->reorderVisual(levels.data(), static_cast<int>(levels.size()),
                               order.data());
      for (const auto logical_index : order) {
        if (logical_index < 0 ||
            static_cast<std::size_t>(logical_index) >= levels.size())
          throw std::runtime_error(
              "RichText visual order exceeds cluster range");
        const auto &unit =
            units[line.begin + static_cast<std::size_t>(logical_index)];
        const Rect bounds{x, finite_extent(y), unit.advance, line_height};
        layout->minimum_width = std::max(layout->minimum_width, unit.advance);
        layout->clusters.push_back({paragraph_index, unit.begin, unit.end,
                                    unit.span, line_index, bounds});
        append_span_rect(layout->span_rectangles[unit.span], bounds);
        float piece_x = x;
        for (std::size_t p = 0; p < unit.pieces.size(); ++p) {
          const auto piece =
              unit.pieces[(unit.level & 1U) ? unit.pieces.size() - 1 - p : p];
          const auto &raw = handler.runs[piece.run];
          RichTextGlyphRun run;
          run.font = raw.font;
          run.paragraph = paragraph_index;
          run.span = unit.span;
          run.line = line_index;
          run.bidi_level = raw.level;
          run.advance = piece.advance;
          run.bounds = {piece_x, bounds.y, piece.advance, line_height};
          run.baseline = baseline;
          decoration_metrics(raw.font, run);
          run.glyphs.assign(
              raw.glyphs.begin() + static_cast<std::ptrdiff_t>(piece.first),
              raw.glyphs.begin() + static_cast<std::ptrdiff_t>(piece.last));
          run.clusters.assign(
              raw.clusters.begin() + static_cast<std::ptrdiff_t>(piece.first),
              raw.clusters.begin() + static_cast<std::ptrdiff_t>(piece.last));
          run.positions.reserve(run.glyphs.size());
          for (auto glyph = piece.first; glyph < piece.last; ++glyph) {
            const auto px = piece_x + raw.positions[glyph].x() -
                            raw.positions[piece.first].x() +
                            raw.offsets[glyph].x();
            const auto py =
                baseline + raw.positions[glyph].y() + raw.offsets[glyph].y();
            if (!std::isfinite(px) || !std::isfinite(py))
              throw std::runtime_error(
                  "RichText shaping returned nonfinite positions");
            run.positions.push_back(SkPoint::Make(px, py));
          }
          layout->runs.push_back(std::move(run));
          piece_x = finite_extent(static_cast<double>(piece_x) + piece.advance);
        }
        x = finite_extent(static_cast<double>(x) + unit.advance);
      }
      maximum = std::max(maximum, line.width);
      y += line_height;
      finite_extent(y);
      ++line_index;
    }
    if (newline == std::string::npos)
      break;
    global = newline + 1;
  } while (global <= source.text.size());
  layout->preferred = {wrap && std::isfinite(width) ? std::min(maximum, width)
                                                    : maximum,
                       finite_extent(y)};
  if (fault == RichTextReflowPhase::Publish)
    throw std::runtime_error("Injected RichText layout publication failure");
  return layout;
}

namespace {
struct RichActionState {
  bool mounted{}, active{true}, allowed{true}, focused{}, hovered{},
      pointer_pressed{}, enter_down{}, space_down{}, space_armed{};
  std::uint64_t generation{};
  std::function<bool()> permission;
  std::function<void()> request_focus;
};
struct RichRuntime {
  std::shared_ptr<const std::vector<RichTextSpan>> spans;
  std::shared_ptr<const RichTextLayout> layout;
  std::vector<std::weak_ptr<RichActionState>> actions;
  Point origin{};
  bool mounted{}, active{true}, allowed{true}, structure_pending{};
  std::uint64_t generation{}, contact_serial{};
  std::optional<std::size_t> pointer;
  PointerId pointer_id{};
  std::function<void()> release_pointer;
};
bool span_visible(const RichRuntime &runtime, std::size_t index) noexcept {
  return runtime.layout && index < runtime.layout->span_rectangles.size() &&
         !runtime.layout->span_rectangles[index].empty();
}
bool contains_span(const RichRuntime &runtime, std::size_t index,
                   Point point) noexcept {
  if (!span_visible(runtime, index))
    return false;
  point.x -= runtime.origin.x;
  point.y -= runtime.origin.y;
  for (const auto rect : runtime.layout->span_rectangles[index])
    if (rect.contains(point))
      return true;
  return false;
}
std::vector<std::size_t> visible_actions(const RichRuntime &runtime) {
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < runtime.spans->size(); ++i)
    if ((*runtime.spans)[i].on_activate && span_visible(runtime, i))
      result.push_back(i);
  return result;
}
void cancel_pointer(std::shared_ptr<RichRuntime> runtime,
                    std::optional<std::uint64_t> expected = {}) {
  if (expected && runtime->contact_serial != *expected)
    return;
  ++runtime->contact_serial;
  if (runtime->pointer && *runtime->pointer < runtime->actions.size())
    if (const auto action = runtime->actions[*runtime->pointer].lock())
      action->pointer_pressed = false;
  runtime->pointer.reset();
  runtime->pointer_id = 0;
  auto release = std::move(runtime->release_pointer);
  if (release)
    release();
}
void cancel_pointer_noexcept(std::shared_ptr<RichRuntime> runtime,
                             std::optional<std::uint64_t> expected = {}) noexcept {
  try {
    cancel_pointer(std::move(runtime), expected);
  } catch (...) {
  }
}
bool action_live(const RichRuntime &runtime, const RichActionState &state,
                 std::uint64_t runtime_generation,
                 std::uint64_t action_generation) noexcept {
  return runtime.mounted && runtime.active && runtime.allowed &&
         runtime.generation == runtime_generation && state.mounted &&
         state.active && state.allowed && state.generation == action_generation;
}
void activate_action(std::shared_ptr<RichRuntime> runtime,
                     std::shared_ptr<RichActionState> state, std::size_t index,
                     std::function<bool()> input_permission,
                     InputContext &context) {
  const auto generation = runtime->generation,
             action_generation = state->generation;
  const auto model = runtime->spans;
  if (!action_live(*runtime, *state, generation, action_generation) ||
      !span_visible(*runtime, index) ||
      (input_permission && !input_permission()) ||
      (state->permission && !state->permission()))
    return;
  // Snapshot application callable using independently-owned inputs. Its copy
  // constructor may retire this component; no component member is read below.
  state->space_armed = false;
  state->pointer_pressed = false;
  cancel_pointer(runtime);
  auto callback = (*model)[index].on_activate;
  auto final_permission = state->permission;
  context.invalidate();
  if (callback &&
      action_live(*runtime, *state, generation, action_generation) &&
      span_visible(*runtime, index) &&
      (!input_permission || input_permission()) &&
      (!final_permission || final_permission()))
    callback();
}
class RichTextComponent;
class RichActionComponent final : public Component {
public:
  RichActionComponent(std::shared_ptr<RichRuntime> runtime, std::size_t index)
      : runtime_(std::move(runtime)), index_(index),
        state_(std::make_shared<RichActionState>()) {}
  bool focusable() const noexcept override {
    return span_visible(*runtime_, index_);
  }
  bool pointer_targetable() const noexcept override { return false; }
  Size measure(const std::vector<ChildMetrics> &) const override { return {}; }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    if (!span_visible(*runtime_, index_))
      return info;
    info.role = SemanticRole::Button;
    info.name = runtime_->layout->span_text[index_];
    info.focusable = true;
    info.focused = state_->focused;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
    if (info.enabled)
      info.actions = {SemanticAction::Activate};
    return info;
  }
  void mount(MountContext &context) override {
    auto permission = InputMutationAccess::action_guard(context);
    auto request = context.focus_requester();
    state_->permission = std::move(permission);
    state_->request_focus = std::move(request);
    state_->mounted = true;
    ++state_->generation;
    runtime_->actions[index_] = state_;
  }
  void activate(LifecycleContext &) override { state_->active = true; }
  void deactivate(LifecycleContext &) override {
    state_->active = false;
    retire(false);
  }
  void unmount(LifecycleContext &) override { retire(true); }
  void focus_changed(bool focused, FocusContext &context) override {
    const auto runtime = runtime_;
    const auto state = state_;
    const auto index = index_;
    const bool changed = state->focused != focused;
    state->focused = focused;
    if (!focused) {
      state->space_armed = false;
      state->space_down = false;
      state->enter_down = false;
      if (runtime->pointer == index)
        cancel_pointer(runtime);
    }
    if (changed)
      context.invalidate();
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Activate)
      return EventResult::Ignored;
    const auto runtime = runtime_;
    const auto state = state_;
    const auto index = index_;
    auto permission = InputMutationAccess::action_guard(context);
    state->space_armed = false;
    state->pointer_pressed = false;
    cancel_pointer(runtime);
    activate_action(runtime, state, index, std::move(permission), context);
    return EventResult::Handled;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto runtime = runtime_;
    const auto state = state_;
    const auto index = index_;
    if (event.type != InputType::KeyDown && event.type != InputType::KeyUp)
      return EventResult::Ignored;
    if (event.key == Key::Escape && event.type == InputType::KeyDown) {
      state->space_armed = false;
      state->pointer_pressed = false;
      cancel_pointer(runtime);
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.key != Key::Space && event.key != Key::Enter)
      return EventResult::Ignored;
    auto permission = InputMutationAccess::action_guard(context);
    bool activate{};
    if (event.key == Key::Enter) {
      if (event.type == InputType::KeyUp)
        state->enter_down = false;
      else if (!state->enter_down) {
        state->enter_down = true;
        activate = true;
      }
    } else if (event.type == InputType::KeyDown) {
      if (!state->space_down) {
        state->space_down = true;
        state->space_armed = true;
        try {
          context.invalidate();
        } catch (...) {
          state->space_armed = false;
          throw;
        }
      }
    } else {
      activate = state->space_down && state->space_armed;
      state->space_down = false;
      state->space_armed = false;
      if (!activate)
        context.invalidate();
    }
    if (activate)
      activate_action(runtime, state, index, std::move(permission), context);
    return EventResult::Handled;
  }
  void paint(PaintContext &) const override {}

private:
  void retire(bool unmounted) noexcept {
    ++state_->generation;
    state_->focused = false;
    state_->hovered = false;
    state_->pointer_pressed = false;
    state_->space_down = false;
    state_->space_armed = false;
    state_->enter_down = false;
    if (unmounted) {
      state_->mounted = false;
      state_->permission = {};
      state_->request_focus = {};
    }
    if (runtime_->actions[index_].lock() == state_ &&
        runtime_->pointer == index_)
      cancel_pointer_noexcept(runtime_);
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &next) noexcept override {
    state_->allowed = next.interactive();
    if (!state_->allowed)
      retire(false);
  }
  std::shared_ptr<RichRuntime> runtime_;
  std::size_t index_{};
  std::shared_ptr<RichActionState> state_;
};
class RichTextComponent final : public Component, public DynamicChildrenSource {
public:
  RichTextComponent(std::vector<RichTextSpan> spans, TextStyle style, bool wrap)
      : style_(std::move(style)), wrap_(wrap),
        runtime_(std::make_shared<RichRuntime>()) {
    runtime_->spans =
        std::make_shared<const std::vector<RichTextSpan>>(std::move(spans));
    runtime_->actions.resize(runtime_->spans->size());
    runtime_->layout = prepare_rich_text_layout(*runtime_->spans, style_,
                                                kUnboundedExtent, wrap_);
    measured_ = runtime_->layout;
    measured_width_ = kUnboundedExtent;
  }
  bool clips_children() const noexcept override { return true; }
  bool pointer_targetable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Text;
    info.text_value = runtime_->layout->repaired_text;
    return info;
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return prepare(kUnboundedExtent)->preferred;
  }
  ChildMetrics
  measure_constrained(const Constraints &constraints,
                      const std::vector<ChildMetrics> &) const override {
    const auto layout = prepare(constraints.max.w);
    auto preferred = constraints.constrain(layout->preferred);
    const auto minimum =
        constraints.constrain(Size{wrap_ ? 0.f : layout->minimum_width, 0});
    return ChildMetrics{minimum, preferred};
  }
  std::optional<float> first_baseline(Size) const override {
    return measured_->first_baseline;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    pending_ = prepare(bounds.w);
    pending_width_ = bounds.w;
    std::vector<std::size_t> installed;
    for (std::size_t i = 0; i < runtime_->actions.size(); ++i)
      if (const auto state = runtime_->actions[i].lock();
          state && state->mounted)
        installed.push_back(i);
    placements.resize(children.size());
    for (std::size_t child = 0;
         child < children.size() && child < installed.size(); ++child) {
      Rect united{};
      for (const auto rect : pending_->span_rectangles[installed[child]])
        united = rich_union(united, rect);
      placements[child].bounds = translated(united, {bounds.x, bounds.y});
    }
  }
  std::vector<std::string> desired_keys() const override {
    std::vector<std::string> result;
    for (const auto i : visible_actions(*runtime_))
      result.push_back((*runtime_->spans)[i].id);
    return result;
  }
  std::vector<DynamicChildSpec> desired_children() const override {
    std::vector<DynamicChildSpec> result;
    for (const auto index : visible_actions(*runtime_)) {
      const auto runtime = runtime_;
      Spec spec{[runtime, index] {
                  return std::make_unique<RichActionComponent>(runtime, index);
                },
                {}};
      result.push_back({(*runtime_->spans)[index].id, std::move(spec)});
    }
    return result;
  }
  void set_structure_invalidator(std::function<void()> invalidator) override {
    structure_ = std::move(invalidator);
  }
  std::vector<Spec> initial_children() const {
    std::vector<Spec> result;
    for (auto &child : desired_children()) {
      child.spec.retained_key = child.key;
      result.push_back(std::move(child.spec));
    }
    return result;
  }
  void mount(MountContext &) override { runtime_->mounted = true; }
  void activate(LifecycleContext &) override {
    runtime_->active = true;
    ++runtime_->generation;
  }
  void deactivate(LifecycleContext &) override {
    runtime_->active = false;
    retire(false);
  }
  void unmount(LifecycleContext &) override {
    runtime_->mounted = false;
    retire(true);
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto runtime = runtime_;
    if (event.type == InputType::PointerCancel) {
      if (runtime->pointer && runtime->pointer_id != event.pointer.id)
        return EventResult::Ignored;
      const bool had = runtime->pointer.has_value();
      cancel_pointer(runtime);
      if (had)
        context.invalidate();
      return had ? EventResult::Handled : EventResult::Ignored;
    }
    if (event.type == InputType::PointerLeave) {
      bool changed{};
      for (auto &weak : runtime->actions)
        if (const auto state = weak.lock()) {
          changed = changed || state->hovered;
          state->hovered = false;
          if (runtime->pointer_id == event.pointer.id)
            state->pointer_pressed = false;
        }
      if (changed)
        context.invalidate();
      return changed ? EventResult::Handled : EventResult::Ignored;
    }
    std::optional<std::size_t> target;
    for (std::size_t i = 0; i < runtime->actions.size(); ++i)
      if (const auto action = runtime->actions[i].lock();
          action && action->mounted && action->allowed &&
          contains_span(*runtime, i, event.position)) {
        target = i;
        break;
      }
    if (event.type == InputType::PointerMove) {
      const auto contact_serial = runtime->contact_serial;
      bool changed{};
      for (std::size_t i = 0; i < runtime->actions.size(); ++i)
        if (const auto state = runtime->actions[i].lock()) {
          const bool hovered = target == i;
          changed = changed || state->hovered != hovered;
          state->hovered = hovered;
          if (runtime->pointer == i && runtime->pointer_id == event.pointer.id)
            state->pointer_pressed = hovered;
        }
      if (changed) {
        try {
          context.invalidate();
        } catch (...) {
          cancel_pointer_noexcept(runtime, contact_serial);
          throw;
        }
      }
      return target || runtime->pointer ? EventResult::Handled
                                        : EventResult::Ignored;
    }
    if (event.type == InputType::PointerDown && target) {
      const auto state = runtime->actions[*target].lock();
      auto permission = InputMutationAccess::action_guard(context);
      auto release = context.pointer_releaser();
      auto focus = state->request_focus;
      const auto generation = runtime->generation,
                 action_generation = state->generation;
      if (!action_live(*runtime, *state, generation, action_generation) ||
          (permission && !permission()) ||
          (state->permission && !state->permission()))
        return EventResult::Ignored;
      const auto previous_contact = runtime->contact_serial;
      auto contact_serial = previous_contact + 1;
      try {
        cancel_pointer(runtime);
        if (runtime->contact_serial != contact_serial ||
            !action_live(*runtime, *state, generation, action_generation) ||
            (permission && !permission()) ||
            (state->permission && !state->permission()))
          return EventResult::Handled;
        contact_serial = ++runtime->contact_serial;
        runtime->pointer = target;
        runtime->pointer_id = event.pointer.id;
        runtime->release_pointer = std::move(release);
        state->pointer_pressed = true;
        state->hovered = true;
        context.capture_pointer();
        if (focus)
          focus();
        context.invalidate();
        if (!action_live(*runtime, *state, generation, action_generation) ||
            (permission && !permission()) ||
            (state->permission && !state->permission()))
          cancel_pointer(runtime, contact_serial);
      } catch (...) {
        cancel_pointer_noexcept(runtime, contact_serial);
        throw;
      }
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerUp && runtime->pointer) {
      if (runtime->pointer_id != event.pointer.id)
        return EventResult::Ignored;
      const auto index = *runtime->pointer;
      const auto state = runtime->actions[index].lock();
      const bool activate = state && target == index && state->pointer_pressed;
      auto permission = InputMutationAccess::action_guard(context);
      cancel_pointer(runtime);
      if (activate)
        activate_action(runtime, state, index, std::move(permission), context);
      else
        context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Escape &&
        runtime->pointer) {
      cancel_pointer(runtime);
      context.invalidate();
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  void paint(PaintContext &context) const override {
    const auto runtime = runtime_;
    const auto layout = runtime->layout;
    const auto model = runtime->spans;
    const auto bounds = context.bounds();
    const Point origin{bounds.x, bounds.y};
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    for (std::size_t i = 0; i < model->size(); ++i)
      if ((*model)[i].background)
        for (const auto rect : layout->span_rectangles[i])
          painter.fill_rounded_rect(translated(rect, origin), 0,
                                    *(*model)[i].background);
    for (const auto &run : layout->runs) {
      const auto &span = (*model)[run.span];
      const auto color = layout->span_styles[run.span].color;
      ShapedTextPaintAccess::draw_glyph_run(
          painter, origin, run.font, run.glyphs, run.positions, run.clusters,
          layout->paragraphs[run.paragraph], color);
      const auto state = runtime->actions[run.span].lock();
      const bool emphasize =
          state &&
          (state->hovered || state->pointer_pressed || state->space_armed);
      if ((span.underline || emphasize) && run.underline_thickness > 0)
        painter.line({origin.x + run.bounds.x,
                      origin.y + run.baseline + run.underline_position},
                     {origin.x + run.bounds.x + run.advance,
                      origin.y + run.baseline + run.underline_position},
                     run.underline_thickness, color);
      if (span.strikethrough && run.strike_thickness > 0)
        painter.line({origin.x + run.bounds.x,
                      origin.y + run.baseline + run.strike_position},
                     {origin.x + run.bounds.x + run.advance,
                      origin.y + run.baseline + run.strike_position},
                     run.strike_thickness, color);
    }
    for (std::size_t i = 0; i < runtime->actions.size(); ++i)
      if (const auto state = runtime->actions[i].lock();
          state && state->focused)
        for (const auto rect : layout->span_rectangles[i]) {
          const auto color = layout->span_styles[i].color;
          painter.stroke_rounded_rect(translated(rect, origin), 0, 1, color);
        }
  }
  std::shared_ptr<const RichTextLayout> committed_layout() const {
    return runtime_->layout;
  }
  void fail_next(RichTextReflowPhase phase) { fault_ = phase; }

private:
  std::shared_ptr<const RichTextLayout> prepare(float width) const {
    if (fault_ == RichTextReflowPhase::None && measured_ &&
        measured_width_ == width)
      return measured_;
    const auto fault = std::exchange(fault_, RichTextReflowPhase::None);
    auto next =
        prepare_rich_text_layout(*runtime_->spans, style_, width, wrap_, fault);
    measured_ = std::move(next);
    measured_width_ = width;
    return measured_;
  }
  void retained_checkpoint() override {
    const auto runtime = runtime_;
    auto structure = structure_;
    if (!runtime->structure_pending || !structure)
      return;
    runtime->structure_pending = false;
    try {
      structure();
    } catch (...) {
      runtime->structure_pending = true;
      throw;
    }
  }
  void layout_committed(Rect, Rect bounds) noexcept override {
    if (!pending_ || pending_width_ != bounds.w)
      return;
    bool changed =
        runtime_->origin.x != bounds.x || runtime_->origin.y != bounds.y;
    const auto &before = runtime_->layout->span_rectangles;
    const auto &after = pending_->span_rectangles;
    for (std::size_t i = 0; i < before.size() && !changed; ++i) {
      changed = before[i].size() != after[i].size();
      for (std::size_t j = 0; j < before[i].size() && !changed; ++j) {
        const auto a = before[i][j], b = after[i][j];
        changed = a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h;
      }
    }
    if (changed) {
      ++runtime_->generation;
      cancel_pointer_noexcept(runtime_);
      for (auto &weak : runtime_->actions)
        if (const auto action = weak.lock()) {
          action->hovered = false;
          action->pointer_pressed = false;
          action->space_armed = false;
        }
    }
    runtime_->layout = pending_;
    runtime_->origin = {bounds.x, bounds.y};
    runtime_->structure_pending = runtime_->structure_pending || changed;
  }
  void retire(bool unmounted) noexcept {
    ++runtime_->generation;
    cancel_pointer_noexcept(runtime_);
    if (unmounted)
      structure_ = {};
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &next) noexcept override {
    runtime_->allowed = next.interactive();
    if (!runtime_->allowed)
      retire(false);
  }
  TextStyle style_;
  bool wrap_{};
  std::shared_ptr<RichRuntime> runtime_;
  std::function<void()> structure_;
  mutable std::shared_ptr<const RichTextLayout> measured_, pending_;
  mutable float measured_width_{}, pending_width_{};
  mutable RichTextReflowPhase fault_{RichTextReflowPhase::None};
};
} // namespace
std::shared_ptr<const RichTextLayout>
RichTextTestAccess::layout(const Component &component) {
  const auto *paragraph = dynamic_cast<const RichTextComponent *>(&component);
  if (!paragraph)
    throw std::invalid_argument(
        "RichText test access requires RichText component");
  return paragraph->committed_layout();
}
void RichTextTestAccess::fail_next_reflow(Component &component,
                                          RichTextReflowPhase phase) {
  auto *paragraph = dynamic_cast<RichTextComponent *>(&component);
  if (!paragraph)
    throw std::invalid_argument(
        "RichText test access requires RichText component");
  paragraph->fail_next(phase);
}
} // namespace detail
RichText::RichText(std::vector<RichTextSpan> spans) : spans_(std::move(spans)) {
  validate_spans(spans_, style_);
}
RichText &&RichText::style(TextStyle value) && {
  validate_text_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
RichText &&RichText::wrap(bool value) && {
  wrap_ = value;
  return std::move(*this);
}
Spec RichText::spec() && {
  Spec result{
      [spans = std::move(spans_), style = std::move(style_), wrap = wrap_] {
        return std::make_unique<detail::RichTextComponent>(spans, style, wrap);
      },
      {}};
  result.children_factory = [](Component &component) {
    return static_cast<detail::RichTextComponent &>(component)
        .initial_children();
  };
  return result;
}
} // namespace ui
