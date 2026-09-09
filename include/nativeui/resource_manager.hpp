#pragma once

#include <nativeui/embedded_resource.hpp>
#include <nativeui/resource.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ui {

/// Non-owning view returned by ResourceManager direct lookup.
///
/// Both the identifier and payload refer to the immutable storage supplied to
/// ResourceManager. That storage must outlive the manager and every ResourceView.
struct ResourceView {
    std::string_view id;
    std::span<const std::byte> bytes;
};

/// Validates and views one sorted immutable embedded-resource table.
///
/// Construction performs a single allocation-free O(N) validation pass. A
/// valid manager then provides allocation-free O(log N) exact-ID lookup over
/// the original table. ResourceManager owns no payload, cache, registry or
/// synchronization state; copies simply borrow the same immutable storage.
/// Concurrent read-only calls are safe while that storage remains alive and
/// immutable.
class ResourceManager {
public:
    explicit ResourceManager(std::span<const EmbeddedResourceEntry> entries) noexcept
        : entries_(entries), error_(validate(entries)) {}

    [[nodiscard]] bool valid() const noexcept { return error_ == ValidationError::None; }

    [[nodiscard]] std::string_view validation_error() const noexcept {
        switch (error_) {
        case ValidationError::None:
            return {};
        case ValidationError::EmptyId:
            return "resource id is empty";
        case ValidationError::NotStrictlyAscending:
            return "resource ids must be strictly ascending and unique";
        }
        return "invalid resource table";
    }

    [[nodiscard]] std::optional<ResourceView> find(std::string_view id) const noexcept {
        if (!valid()) return std::nullopt;

        const auto it = std::lower_bound(
            entries_.begin(), entries_.end(), id,
            [](const EmbeddedResourceEntry& entry, std::string_view needle) noexcept {
                return compare_ids(entry.id, needle) < 0;
            });
        if (it == entries_.end() || compare_ids(it->id, id) != 0) return std::nullopt;
        return ResourceView{it->id, it->bytes};
    }

    [[nodiscard]] bool contains(std::string_view id) const noexcept {
        return find(id).has_value();
    }

    [[nodiscard]] std::span<const EmbeddedResourceEntry> resources() const noexcept {
        return valid() ? entries_ : std::span<const EmbeddedResourceEntry>{};
    }

private:
    enum class ValidationError : unsigned char {
        None,
        EmptyId,
        NotStrictlyAscending,
    };

    [[nodiscard]] static constexpr int compare_ids(std::string_view lhs,
                                                   std::string_view rhs) noexcept {
        const std::size_t common = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
        for (std::size_t index = 0; index < common; ++index) {
            const auto left = static_cast<unsigned char>(lhs[index]);
            const auto right = static_cast<unsigned char>(rhs[index]);
            if (left < right) return -1;
            if (left > right) return 1;
        }
        if (lhs.size() < rhs.size()) return -1;
        if (lhs.size() > rhs.size()) return 1;
        return 0;
    }

    [[nodiscard]] static constexpr ValidationError validate(
        std::span<const EmbeddedResourceEntry> entries) noexcept {
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (entries[index].id.empty()) return ValidationError::EmptyId;
            if (index > 0 && compare_ids(entries[index - 1].id, entries[index].id) >= 0) {
                return ValidationError::NotStrictlyAscending;
            }
        }
        return ValidationError::None;
    }

    std::span<const EmbeddedResourceEntry> entries_;
    ValidationError error_{ValidationError::None};
};

/// Compatibility adapter for APIs that require owned resource bytes.
///
/// ResourceManagerProvider stores a lightweight ResourceManager copy. A
/// successful non-empty load allocates/copies the requested payload into the
/// vector required by ResourceProvider; it is therefore a UI/resource-
/// preparation operation and is not real-time safe. Empty resources still
/// return an engaged empty vector. Missing/invalid resources return std::nullopt
/// and no cache is kept here.
class ResourceManagerProvider final : public ResourceProvider {
public:
    explicit ResourceManagerProvider(ResourceManager manager) noexcept : manager_(manager) {}

    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) override {
        const auto resource = manager_.find(resource_id);
        if (!resource) return std::nullopt;
        if (resource->bytes.empty()) return std::vector<std::byte>{};
        return std::vector<std::byte>{resource->bytes.begin(), resource->bytes.end()};
    }

private:
    ResourceManager manager_;
};

} // namespace ui
