#pragma once

#include <nativeui/detail/semantic_native_publication.hpp>
#include <nativeui/detail/semantic_virtual_source.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace ui::detail {

/// One exact immutable native-reader result.
///
/// Platform accessibility adapters retain the native publication itself rather
/// than loading semantic data and geometry independently.  The semantic node or
/// virtual item resolved below therefore always belongs to the same generation
/// as the copied T043 screen geometry exposed by geometry().
class SemanticNativeSnapshotRead final {
public:
    [[nodiscard]] std::uint64_t generation() const noexcept {
        return publication_ ? publication_->generation : 0;
    }

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return publication_ ? publication_->semantic_generation() : 0;
    }

    [[nodiscard]] SemanticId node_id() const noexcept {
        return node_id_;
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_token() const noexcept {
        return virtual_item_ ? std::optional<VirtualSemanticItemToken>{virtual_item_->token}
                             : std::nullopt;
    }

    [[nodiscard]] const SemanticInfo& info() const noexcept {
        if (virtual_item_) {
            return virtual_item_->info;
        }
        return publication_->semantic_snapshot->nodes[node_index_].info;
    }

    /// Bounds remain logical/view-relative here by design.  A platform adapter
    /// applies geometry() exactly once at its native coordinate boundary.
    [[nodiscard]] Rect logical_bounds() const noexcept {
        if (virtual_item_) {
            return virtual_item_->logical_bounds;
        }
        return publication_->semantic_snapshot->nodes[node_index_].bounds;
    }

    [[nodiscard]] const SemanticNativeGeometry& geometry() const noexcept {
        return publication_->geometry;
    }

    [[nodiscard]] SemanticId parent_id() const noexcept {
        if (virtual_item_) {
            return node_id_;
        }
        return publication_->semantic_snapshot->nodes[node_index_].parent;
    }

    /// Resolve the parent's role from the same immutable publication retained by
    /// this read. Virtual items are owned by the ordinary list node represented
    /// by node_index_; ordinary nodes resolve their parent from the immutable
    /// ordinary-node table. A root or malformed parent reference has no role.
    [[nodiscard]] std::optional<SemanticRole> parent_role() const noexcept {
        if (!publication_ || !publication_->semantic_snapshot) {
            return std::nullopt;
        }

        const SemanticId parent = parent_id();
        if (parent == kInvalidSemanticId) {
            return std::nullopt;
        }

        const auto& nodes = publication_->semantic_snapshot->nodes;
        if (virtual_item_) {
            if (node_index_ >= nodes.size() || nodes[node_index_].id != parent) {
                return std::nullopt;
            }
            return nodes[node_index_].info.role;
        }

        for (const auto& node : nodes) {
            if (node.id == parent) {
                return node.info.role;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::size_t child_count() const noexcept {
        if (virtual_item_) {
            return 0;
        }
        return publication_->semantic_snapshot->nodes[node_index_].children.size();
    }

    [[nodiscard]] std::optional<SemanticId> child_at(std::size_t index) const noexcept {
        if (virtual_item_) {
            return std::nullopt;
        }
        const auto& children = publication_->semantic_snapshot->nodes[node_index_].children;
        if (index >= children.size()) {
            return std::nullopt;
        }
        return children[index];
    }

    /// Resolve an ordinary child identity back to its ordered index inside this
    /// exact immutable publication. Ordinary semantic child sets are small and
    /// already stored as an ordered vector, so this bounded linear lookup avoids
    /// adding another per-generation index or mutable cache.
    [[nodiscard]] std::optional<std::size_t> ordinary_child_index_of(
        SemanticId child_id) const noexcept {
        if (virtual_item_ || child_id == kInvalidSemanticId) {
            return std::nullopt;
        }

        const auto& children = publication_->semantic_snapshot->nodes[node_index_].children;
        for (std::size_t index = 0U; index < children.size(); ++index) {
            if (children[index] == child_id) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::size_t virtual_child_count() const noexcept {
        if (virtual_item_) {
            return 0;
        }
        const auto& virtual_children =
            publication_->semantic_snapshot->nodes[node_index_].virtual_children;
        return virtual_children ? virtual_children->size() : 0;
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_child_token_at(
        std::size_t index) const noexcept {
        if (virtual_item_) {
            return std::nullopt;
        }
        const auto& virtual_children =
            publication_->semantic_snapshot->nodes[node_index_].virtual_children;
        if (!virtual_children) {
            return std::nullopt;
        }
        const auto& metadata = virtual_children->metadata_snapshot();
        if (!metadata || index >= metadata->size()) {
            return std::nullopt;
        }
        return (*metadata)[index].token;
    }

    /// Resolve a virtual token back to its logical index through the immutable
    /// token index retained by this exact publication. Missing or inconsistent
    /// indexing fails closed; native callbacks must never fall back to scanning
    /// a large virtual collection.
    [[nodiscard]] std::optional<std::size_t> virtual_child_index_of(
        VirtualSemanticItemToken token) const {
        if (virtual_item_ || token == kInvalidVirtualSemanticItemToken) {
            return std::nullopt;
        }

        const auto& virtual_children =
            publication_->semantic_snapshot->nodes[node_index_].virtual_children;
        if (!virtual_children) {
            return std::nullopt;
        }

        const auto& metadata = virtual_children->metadata_snapshot();
        const auto& token_index = virtual_children->token_index_snapshot();
        if (!metadata || !token_index) {
            return std::nullopt;
        }

        const auto found = token_index->find(token);
        if (found == token_index->end() || found->second >= metadata->size() ||
            (*metadata)[found->second].token != token) {
            return std::nullopt;
        }
        return found->second;
    }

    /// Resolve the role of one ordinary child from this exact publication.
    /// No later publication-source load occurs, so native proxy construction can
    /// keep the child identity and its initial role in the same generation.
    [[nodiscard]] std::optional<SemanticRole> ordinary_child_role_at(
        std::size_t index) const noexcept {
        if (virtual_item_ || !publication_ || !publication_->semantic_snapshot) {
            return std::nullopt;
        }

        const auto child_id = child_at(index);
        if (!child_id || *child_id == kInvalidSemanticId) {
            return std::nullopt;
        }

        const auto& nodes = publication_->semantic_snapshot->nodes;
        for (const auto& node : nodes) {
            if (node.id == *child_id) {
                return node.info.role;
            }
        }
        return std::nullopt;
    }

    /// Resolve a virtual child's role through the immutable indexed metadata
    /// retained by this publication. Work remains O(1) in collection size and
    /// never falls back to the generic linear token scan on a native callback.
    [[nodiscard]] std::optional<SemanticRole> virtual_child_role_at(
        std::size_t index) const {
        if (virtual_item_ || !publication_ || !publication_->semantic_snapshot) {
            return std::nullopt;
        }

        const auto& virtual_children =
            publication_->semantic_snapshot->nodes[node_index_].virtual_children;
        const auto token = virtual_child_token_at(index);
        if (!virtual_children || !token ||
            *token == kInvalidVirtualSemanticItemToken) {
            return std::nullopt;
        }

        const auto item = resolve_indexed_virtual_semantic_item(
            *virtual_children, *token);
        return item ? std::optional<SemanticRole>{item->info.role} : std::nullopt;
    }

private:
    friend class SemanticNativeSnapshotQuery;

    SemanticNativeSnapshotRead(
        std::shared_ptr<const SemanticNativePublicationSnapshot> publication,
        std::size_t node_index,
        SemanticId node_id,
        std::optional<VirtualSemanticItem> virtual_item)
        : publication_(std::move(publication)),
          node_index_(node_index),
          node_id_(node_id),
          virtual_item_(std::move(virtual_item)) {}

    std::shared_ptr<const SemanticNativePublicationSnapshot> publication_;
    std::size_t node_index_{};
    SemanticId node_id_{kInvalidSemanticId};
    std::optional<VirtualSemanticItem> virtual_item_;
};

/// Resolve ordinary or virtual semantic identity from one already-retained
/// native publication.  This is the platform-adapter read seam: callers load
/// SemanticNativeViewBridge::native_current() once, resolve through this type,
/// and never pair a later semantic load with older/newer geometry.
class SemanticNativeSnapshotQuery final {
public:
    [[nodiscard]] static std::optional<SemanticNativeSnapshotRead> ordinary(
        std::shared_ptr<const SemanticNativePublicationSnapshot> publication,
        SemanticId node_id) {
        return resolve(std::move(publication), node_id, std::nullopt);
    }

    [[nodiscard]] static std::optional<SemanticNativeSnapshotRead> virtual_item(
        std::shared_ptr<const SemanticNativePublicationSnapshot> publication,
        SemanticId list_node_id,
        VirtualSemanticItemToken token) {
        return resolve(std::move(publication), list_node_id, token);
    }

private:
    [[nodiscard]] static std::optional<SemanticNativeSnapshotRead> resolve(
        std::shared_ptr<const SemanticNativePublicationSnapshot> publication,
        SemanticId node_id,
        std::optional<VirtualSemanticItemToken> virtual_token) {
        if (!publication || !publication->semantic_snapshot ||
            node_id == kInvalidSemanticId) {
            return std::nullopt;
        }

        const auto& snapshot = *publication->semantic_snapshot;
        for (std::size_t node_index = 0; node_index < snapshot.nodes.size(); ++node_index) {
            const auto& node = snapshot.nodes[node_index];
            if (node.id != node_id) {
                continue;
            }

            if (!virtual_token) {
                return SemanticNativeSnapshotRead{
                    std::move(publication), node_index, node_id, std::nullopt};
            }

            if (!node.virtual_children ||
                *virtual_token == kInvalidVirtualSemanticItemToken) {
                return std::nullopt;
            }

            // Keep native virtual-item lookup O(1).  Missing/inconsistent T067
            // token indexing is defunct rather than falling back to the generic
            // linear metadata scan on an OS callback thread.
            auto item = resolve_indexed_virtual_semantic_item(
                *node.virtual_children, *virtual_token);
            if (!item) {
                return std::nullopt;
            }

            return SemanticNativeSnapshotRead{
                std::move(publication), node_index, node_id, std::move(item)};
        }

        return std::nullopt;
    }
};

} // namespace ui::detail
