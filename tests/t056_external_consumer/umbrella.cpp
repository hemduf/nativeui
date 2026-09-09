#include <nativeui/nativeui.hpp>

#include <cstddef>
#include <span>
#include <string_view>

ui::EmbeddedResourceEntry make_umbrella_entry() {
  return ui::EmbeddedResourceEntry{std::string_view{}, std::span<const std::byte>{}};
}
