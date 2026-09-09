#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace ui {

struct EmbeddedResourceEntry {
  std::string_view id;
  std::span<const std::byte> bytes;
};

} // namespace ui
