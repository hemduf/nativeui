#pragma once

#include <nativeui/detail/semantic_snapshot.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace ui::detail {

class SemanticSnapshotRead {
public:
    [[nodiscard]] std::uint64_t generation() const noexcept {
        return snapshot_ ? snapshot_->generation : 0;
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
        return snapshot_->nodes[node_index_].info;
    }

    [[nodiscard]] Rect bounds() const noexcept {
        if (virtual_item_) {
            return virtual_item_->logical_bounds;
        }
        return snapshot_->nodes[node_index_].bounds;
    }

private:
    friend class SemanticSnapshotProxy;

    SemanticSnapshotRead(std::shared_ptr<const SemanticTreeSnapshot> snapshot,
                         std::size_t node_index,
                         SemanticId node_id,
                         std::optional<VirtualSemanticItem> virtual_item)
        : snapshot_(std::move(snapshot)),
          node_index_(node_index),
          node_id_(node_id),
          virtual_item_(std::move(virtual_item)) {}

    std::shared_ptr<const SemanticTreeSnapshot> snapshot_;
    std::size_t node_index_{};
    SemanticId node_id_{kInvalidSemanticId};
    std::optional<VirtualSemanticItem> virtual_item_;
};

class SemanticSnapshotProxy {
public:
    [[nodiscard]] static SemanticSnapshotProxy ordinary(
        const std::shared_ptr<SemanticSnapshotPublisher>& publisher,
        SemanticId node_id) noexcept {
        return SemanticSnapshotProxy{publisher, node_id, std::nullopt};
    }

    [[nodiscard]] static SemanticSnapshotProxy virtual_item(
        const std::shared_ptr<SemanticSnapshotPublisher>& publisher,
        SemanticId list_node_id,
        VirtualSemanticItemToken token) noexcept {
        return SemanticSnapshotProxy{publisher, list_node_id, token};
    }

    [[nodiscard]] std::optional<SemanticSnapshotRead> read() const {
        const auto publisher = publisher_.lock();
        if (!publisher) {
            return std::nullopt;
        }

        auto snapshot = publisher->current();
        if (!snapshot) {
            return std::nullopt;
        }

        for (std::size_t node_index = 0; node_index < snapshot->nodes.size(); ++node_index) {
            const auto& node = snapshot->nodes[node_index];
            if (node.id != node_id_) {
                continue;
            }

            if (!virtual_token_) {
                return SemanticSnapshotRead{
                    std::move(snapshot), node_index, node_id_, std::nullopt};
            }

            if (!node.virtual_children) {
                return std::nullopt;
            }

            const auto& metadata = node.virtual_children->metadata_snapshot();
            if (!metadata) {
                return std::nullopt;
            }

            for (std::size_t item_index = 0; item_index < metadata->size(); ++item_index) {
                if ((*metadata)[item_index].token != *virtual_token_) {
                    continue;
                }

                auto item = node.virtual_children->item_at(item_index);
                if (!item) {
                    return std::nullopt;
                }

                return SemanticSnapshotRead{
                    std::move(snapshot), node_index, node_id_, std::move(item)};
            }

            return std::nullopt;
        }

        return std::nullopt;
    }

private:
    SemanticSnapshotProxy(const std::shared_ptr<SemanticSnapshotPublisher>& publisher,
                          SemanticId node_id,
                          std::optional<VirtualSemanticItemToken> virtual_token) noexcept
        : publisher_(publisher),
          node_id_(node_id),
          virtual_token_(virtual_token) {}

    std::weak_ptr<SemanticSnapshotPublisher> publisher_;
    SemanticId node_id_{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token_;
};

} // namespace ui::detail
