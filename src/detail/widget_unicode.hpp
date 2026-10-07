#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ui::detail {
struct WidgetTextSpan {
  std::size_t begin{};
  std::size_t end{};
};
[[nodiscard]] std::string widget_repair_utf8(std::string_view value);
// Spans always index the supplied valid UTF8 text in bytes. Backend objects
// and locale state never escape this implementation boundary.
[[nodiscard]] std::vector<WidgetTextSpan>
widget_graphemes(std::string_view value);
[[nodiscard]] std::vector<WidgetTextSpan> widget_words(std::string_view value);
[[nodiscard]] std::vector<std::size_t>
widget_line_breaks(std::string_view value);
[[nodiscard]] std::string widget_initials(std::string_view repaired_name);
} // namespace ui::detail
