#include <nativeui/embedded_resource.hpp>

#include <cstddef>
#include <span>
#include <string_view>

static_assert(sizeof(ui::EmbeddedResourceEntry) > 0);

ui::EmbeddedResourceEntry make_entry() {
  return ui::EmbeddedResourceEntry{std::string_view{}, std::span<const std::byte>{}};
}
