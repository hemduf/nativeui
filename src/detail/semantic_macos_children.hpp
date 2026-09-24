#pragma once

#include <nativeui/detail/semantic_native_query.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace ui::detail {

/// Stable semantic identity used when a macOS accessibility callback asks for
/// one child without materializing the complete native child set.
///
/// Ordinary children carry their own SemanticId. Virtual children retain the
/// owning ListView SemanticId plus the immutable logical token minted by the
/// virtual collection. No native object or live retained-tree pointer is stored.
struct MacOSAccessibilityChildIdentity final {
    SemanticId node_id{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token;

    [[nodiscard]] bool valid() const noexcept {
        if (node_id == kInvalidSemanticId) {
            return false;
        }
        return !virtual_token ||
               *virtual_token != kInvalidVirtualSemanticItemToken;
    }

    bool operator==(const MacOSAccessibilityChildIdentity&) const = default;
};

/// Callback-local child projection retained from one exact immutable native
/// publication generation.
///
/// The projection deliberately keeps ordinary and virtual children separate:
/// their final NSAccessibility ordering is a platform-bridge concern, while
/// indexed virtual access must remain lazy and must never allocate one native
/// provider per logical row. Every method below is bounded and reads only the
/// already-retained immutable snapshot.
class MacOSAccessibilityChildProjection final {
public:
    explicit MacOSAccessibilityChildProjection(SemanticNativeSnapshotRead read)
        : read_(std::move(read)) {}

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return read_.generation();
    }

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return read_.semantic_generation();
    }

    /// Resolve the stable semantic parent from the same immutable publication
    /// already retained for this child. Virtual rows report their owning list as
    /// an ordinary parent identity. A semantic root has no parent.
    [[nodiscard]] std::optional<MacOSAccessibilityChildIdentity>
    parent_identity() const noexcept {
        const SemanticId parent_id = read_.parent_id();
        if (parent_id == kInvalidSemanticId) {
            return std::nullopt;
        }
        return MacOSAccessibilityChildIdentity{parent_id, std::nullopt};
    }

    /// Resolve the semantic role of the parent from the same retained
    /// publication as parent_identity(). This lets the native cache construct a
    /// missing parent proxy without reloading a newer generation.
    [[nodiscard]] std::optional<SemanticRole> parent_role() const noexcept {
        return read_.parent_role();
    }

    [[nodiscard]] std::size_t ordinary_child_count() const noexcept {
        return read_.child_count();
    }

    [[nodiscard]] std::size_t virtual_child_count() const noexcept {
        return read_.virtual_child_count();
    }

    [[nodiscard]] std::optional<MacOSAccessibilityChildIdentity>
    ordinary_child_identity_at(std::size_t index) const noexcept {
        const auto child_id = read_.child_at(index);
        if (!child_id || *child_id == kInvalidSemanticId) {
            return std::nullopt;
        }
        return MacOSAccessibilityChildIdentity{*child_id, std::nullopt};
    }

    [[nodiscard]] std::optional<MacOSAccessibilityChildIdentity>
    virtual_child_identity_at(std::size_t index) const noexcept {
        const auto token = read_.virtual_child_token_at(index);
        if (!token || *token == kInvalidVirtualSemanticItemToken ||
            read_.node_id() == kInvalidSemanticId) {
            return std::nullopt;
        }
        return MacOSAccessibilityChildIdentity{read_.node_id(), *token};
    }

    /// Resolve an ordinary stable identity back to its current ordered position
    /// in this retained publication.
    [[nodiscard]] std::optional<std::size_t> ordinary_child_index_of(
        SemanticId child_id) const noexcept {
        return read_.ordinary_child_index_of(child_id);
    }

    /// Resolve a virtual token through the retained immutable token index. This
    /// never scans the logical collection and therefore remains bounded for very
    /// large virtual datasets.
    [[nodiscard]] std::optional<std::size_t> virtual_child_index_of(
        VirtualSemanticItemToken token) const {
        return read_.virtual_child_index_of(token);
    }

    /// Resolve an ordinary child's role from the same immutable generation as
    /// its identity. This is the cache-miss initialization seam for AppKit.
    [[nodiscard]] std::optional<SemanticRole>
    ordinary_child_role_at(std::size_t index) const noexcept {
        return read_.ordinary_child_role_at(index);
    }

    /// Resolve a virtual child's role through the retained indexed metadata
    /// without loading a newer native publication or walking the collection.
    [[nodiscard]] std::optional<SemanticRole>
    virtual_child_role_at(std::size_t index) const {
        return read_.virtual_child_role_at(index);
    }

private:
    SemanticNativeSnapshotRead read_;
};

} // namespace ui::detail
