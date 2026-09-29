#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace ui {

/// Supplies encoded resource bytes by application-defined identifier.
///
/// NativeUI deliberately leaves storage policy to the application. Providers
/// can source bytes from files, bundles, archives, generated memory or any
/// other backend without coupling widgets or resource caches to filesystem I/O.
/// Implementations are called from the UI/resource-preparation domain unless a
/// concrete provider explicitly documents a stronger thread-safety contract.
class ResourceProvider {
public:
    virtual ~ResourceProvider() = default;

    [[nodiscard]] virtual std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) = 0;
};

} // namespace ui
