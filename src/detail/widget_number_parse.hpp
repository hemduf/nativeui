#pragma once

#include <optional>
#include <string_view>

namespace ui::detail {
// Complete finite ASCII decimal conversion with an instance-local classic
// locale. Conversion/resource failure rejects the draft without publication.
std::optional<double> parse_decimal_number(std::string_view text) noexcept;
} // namespace ui::detail
