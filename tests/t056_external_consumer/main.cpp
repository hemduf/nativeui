#include <nativeui/nativeui.hpp>
#include <nativeui_binary_data/T056PackageResources/resources.hpp>

#include <cstddef>
#include <string_view>

#ifndef T056_POST_HELPER
#error "post-helper target customization was not preserved"
#endif

int main() {
  const auto entries = t056_external::resources::table();
  if (entries.size() != 2) return 1;
  if (entries[0].id != "message") return 2;
  if (entries[1].id != "punct;../semi") return 3;

  constexpr std::string_view message = "hello from installed NativeUI\n";
  if (entries[0].bytes.size() != message.size()) return 4;
  for (std::size_t i = 0; i < message.size(); ++i) {
    if (std::to_integer<unsigned char>(entries[0].bytes[i]) !=
        static_cast<unsigned char>(message[i])) return 5;
  }

  constexpr std::string_view other = "second resource\n";
  if (entries[1].bytes.size() != other.size()) return 6;
  for (std::size_t i = 0; i < other.size(); ++i) {
    if (std::to_integer<unsigned char>(entries[1].bytes[i]) !=
        static_cast<unsigned char>(other[i])) return 7;
  }

  // T057 consumes the exact sorted immutable table produced by T056 without
  // copying or re-indexing it. This fixture runs for both build-tree and
  // relocated install-tree packages on every CI platform.
  const ui::ResourceManager manager{entries};
  if (!manager.valid()) return 8;
  if (manager.resources().data() != entries.data()) return 9;
  const auto direct_message = manager.find("message");
  if (!direct_message || direct_message->bytes.data() != entries[0].bytes.data()) return 10;
  const auto direct_semicolon = manager.find("punct;../semi");
  if (!direct_semicolon || direct_semicolon->bytes.data() != entries[1].bytes.data()) return 11;

  ui::ResourceManagerProvider provider{manager};
  const auto owned_message = provider.load("message");
  if (!owned_message || owned_message->size() != message.size()) return 12;
  if (owned_message->data() == entries[0].bytes.data()) return 13;
  if (provider.load("missing").has_value()) return 14;

  return 0;
}
