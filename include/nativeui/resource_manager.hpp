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
    /// Borrowed identifier of the matched table entry.
    ///
    /// This view aliases the original EmbeddedResourceEntry::id storage; it is
    /// not copied and is invalid once that backing storage ceases to exist.
    std::string_view id;

    /// Borrowed payload of the matched table entry.
    ///
    /// The span may be empty for a present empty resource. It aliases the
    /// original embedded payload and must not outlive that immutable storage.
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
    /// Borrow and validate one immutable resource table.
    ///
    /// `entries` itself and the identifier/payload storage referenced by every
    /// entry remain caller-owned and must outlive this manager and every view
    /// returned from it. Construction performs one allocation-free O(N) pass
    /// and stores only the borrowed span plus a validation result.
    ///
    /// An empty table is valid. Otherwise every ID must be non-empty and IDs
    /// must be strictly ascending and unique using exact unsigned-byte
    /// lexicographic ordering. Invalid input is recorded atomically: lookup does
    /// not expose a valid prefix of an invalid table.
    explicit ResourceManager(std::span<const EmbeddedResourceEntry> entries) noexcept
        : entries_(entries), error_(validate(entries)) {}

    /// Whether the complete borrowed table satisfied the constructor contract.
    ///
    /// This check is allocation-free, thread-safe for concurrent read-only use,
    /// and never revalidates or dereferences payload bytes.
    [[nodiscard]] bool valid() const noexcept { return error_ == ValidationError::None; }

    /// Return the stable validation diagnostic recorded at construction.
    ///
    /// The result is empty for a valid table. Otherwise it is a borrowed view of
    /// static diagnostic text owned by NativeUI, not by the resource table.
    /// Calling this function performs no allocation and cannot throw.
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

    /// Find one resource by exact identifier without copying its payload.
    ///
    /// Lookup is case-sensitive, performs no normalization, and uses the same
    /// unsigned-byte ordering required by construction. A valid table is
    /// searched in O(log N) comparisons without allocation or synchronization.
    ///
    /// Returns std::nullopt when the table is invalid or the ID is absent.
    /// A present resource with zero payload bytes returns an engaged ResourceView
    /// whose bytes span is empty. The returned id/bytes are borrowed from the
    /// original table storage and must not outlive it.
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

    /// Test exact-ID presence using the same allocation-free lookup as find().
    ///
    /// Returns false for an invalid table and for a missing ID; returns true for
    /// a present resource even when its payload is empty.
    [[nodiscard]] bool contains(std::string_view id) const noexcept {
        return find(id).has_value();
    }

    /// Expose the complete validated table as a borrowed read-only span.
    ///
    /// Invalid managers return an empty span so callers cannot accidentally
    /// consume a valid-looking prefix. A valid empty table also returns an empty
    /// span; use valid() when that distinction matters. The returned span aliases
    /// the caller-owned entries supplied to the constructor.
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
    /// Adapt a ResourceManager to the owning ResourceProvider interface.
    ///
    /// The manager copy remains a non-owning view; constructing this adapter does
    /// not extend the lifetime of the source table, IDs or payload bytes.
    explicit ResourceManagerProvider(ResourceManager manager) noexcept : manager_(manager) {}

    /// Load one exact-ID resource into independently owned bytes.
    ///
    /// Missing resources and invalid underlying tables return std::nullopt.
    /// Present empty resources return an engaged empty vector. Non-empty
    /// resources are copied, so the returned vector remains valid after the
    /// source table is later destroyed, but allocation failure can propagate.
    ///
    /// The adapter keeps no mutable cache. Concurrent calls are safe while the
    /// underlying borrowed table remains alive and immutable. Because successful
    /// non-empty loads allocate/copy, this compatibility path is resource-
    /// preparation/UI-side work and is not audio-real-time safe.
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
