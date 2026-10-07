#include "detail/widget_unicode.hpp"
#include <nativeui/text.hpp>

#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ui::detail {
std::string widget_repair_utf8(std::string_view value) {
  if (text::utf8_prefix(value))
    return std::string{value};
  std::string repaired;
  repaired.reserve(value.size());
  for (std::size_t offset = 0; offset < value.size();) {
    std::string_view scalar;
    for (std::size_t limit = 1; limit <= 4; ++limit) {
      const auto candidate = text::utf8_prefix(value.substr(offset), limit);
      if (candidate && !candidate->empty()) {
        scalar = *candidate;
        break;
      }
    }
    if (scalar.empty()) {
      // Match TextService's repair policy: one replacement per malformed byte,
      // then continue decoding the remaining valid suffix.
      repaired.append("\xef\xbf\xbd");
      ++offset;
    } else {
      repaired.append(scalar);
      offset += scalar.size();
    }
  }
  return repaired;
}
namespace {
std::vector<WidgetTextSpan> breaks(std::string_view value,
                                   SkUnicode::BreakType type,
                                   bool meaningful_words = false) {
  if (!text::utf8_prefix(value))
    throw std::invalid_argument(
        "Widget Unicode boundaries require valid UTF8 text");
  if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw std::length_error("Widget text exceeds the Unicode iterator domain");
  if (value.empty())
    return {};
  auto unicode = SkUnicodes::ICU::Make();
  if (!unicode)
    throw std::runtime_error("Widget Unicode backend is unavailable");
  auto iterator = unicode->makeBreakIterator("", type);
  if (!iterator ||
      !iterator->setText(value.data(), static_cast<int>(value.size())))
    throw std::runtime_error("Widget Unicode iterator could not accept text");
  int previous = iterator->first();
  if (previous != 0)
    throw std::runtime_error("Widget Unicode iterator has no initial boundary");
  std::vector<WidgetTextSpan> output;
  while (static_cast<std::size_t>(previous) < value.size()) {
    const int next = iterator->next();
    if (next <= previous || static_cast<std::size_t>(next) > value.size())
      throw std::runtime_error(
          "Widget Unicode iterator returned an invalid boundary");
    // ICU word statuses [100,500) identify numbers, letters, kana and
    // ideographs. Delimiters have statuses below100 and are not avatar words.
    const auto status = iterator->status();
    if (!meaningful_words || (status >= 100 && status < 500))
      output.push_back(
          {static_cast<std::size_t>(previous), static_cast<std::size_t>(next)});
    previous = next;
  }
  return output;
}
bool usable_initial(std::string_view grapheme) noexcept {
  if (grapheme.empty() || grapheme.starts_with("\xef\xbf\xbd"))
    return false;
  const auto first = static_cast<unsigned char>(grapheme.front());
  if (first >= 0x80)
    return true;
  return (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') ||
         (first >= '0' && first <= '9');
}
std::string first_initial(std::string_view value, WidgetTextSpan word,
                          const std::vector<WidgetTextSpan> &graphemes) {
  const auto first =
      std::lower_bound(graphemes.begin(), graphemes.end(), word.begin,
                       [](WidgetTextSpan span, std::size_t begin) {
                         return span.begin < begin;
                       });
  for (auto it = first; it != graphemes.end() && it->begin < word.end; ++it) {
    const auto grapheme = value.substr(it->begin, it->end - it->begin);
    if (!usable_initial(grapheme))
      continue;
    std::string result{grapheme};
    if (result.front() >= 'a' && result.front() <= 'z')
      result.front() = static_cast<char>(result.front() - 'a' + 'A');
    return result;
  }
  return {};
}
} // namespace
std::vector<WidgetTextSpan> widget_graphemes(std::string_view value) {
  return breaks(value, SkUnicode::BreakType::kGraphemes);
}
std::vector<WidgetTextSpan> widget_words(std::string_view value) {
  return breaks(value, SkUnicode::BreakType::kWords, true);
}
std::vector<std::size_t> widget_line_breaks(std::string_view value) {
  const auto spans = breaks(value, SkUnicode::BreakType::kLines);
  std::vector<std::size_t> result;
  result.reserve(spans.size());
  for (const auto &span : spans)
    result.push_back(span.end);
  return result;
}
std::string widget_initials(std::string_view repaired_name) {
  const auto words = widget_words(repaired_name);
  if (words.empty())
    return "?";
  const auto graphemes = widget_graphemes(repaired_name);
  std::string first;
  std::size_t first_word{};
  for (; first_word < words.size(); ++first_word) {
    first = first_initial(repaired_name, words[first_word], graphemes);
    if (!first.empty())
      break;
  }
  if (first.empty())
    return "?";
  for (std::size_t last_word = words.size(); last_word-- > first_word + 1;) {
    auto last = first_initial(repaired_name, words[last_word], graphemes);
    if (!last.empty())
      return first + last;
  }
  return first;
}
} // namespace ui::detail
