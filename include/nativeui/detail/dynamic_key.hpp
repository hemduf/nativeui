#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ui::detail {

template <class>
inline constexpr bool kUnsupportedDynamicKey = false;

template <class Key>
[[nodiscard]] std::string encode_dynamic_key(Key&& key) {
    using Value = std::remove_cvref_t<Key>;
    if constexpr (std::is_same_v<Value, std::string>) {
        return "s:" + std::forward<Key>(key);
    } else if constexpr (std::is_convertible_v<Key, std::string_view>) {
        return "s:" + std::string{std::string_view{std::forward<Key>(key)}};
    } else if constexpr (std::is_enum_v<Value>) {
        return encode_dynamic_key(static_cast<std::underlying_type_t<Value>>(key));
    } else if constexpr (std::is_integral_v<Value> && std::is_signed_v<Value>) {
        return "i:" + std::to_string(static_cast<long long>(key));
    } else if constexpr (std::is_integral_v<Value>) {
        return "u:" + std::to_string(static_cast<unsigned long long>(key));
    } else {
        static_assert(kUnsupportedDynamicKey<Value>,
                      "NativeUI dynamic keys must be string-like, integral, or enum values");
    }
}

} // namespace ui::detail
