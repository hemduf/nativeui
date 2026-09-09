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
  if (entries[1].id != "punct;semi") return 3;

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

  return 0;
}
