#pragma once

#include <nativeui/detail/semantic_snapshot.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
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

namespace semantic_proxy_detail {

struct CacheKey {
    SemanticId node_id{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token;

    bool operator==(const CacheKey&) const = default;
};

struct CacheKeyHash {
    [[nodiscard]] std::size_t operator()(const CacheKey& key) const noexcept {
        std::size_t seed = std::hash<SemanticId>{}(key.node_id);
        const auto mix = [&seed](std::size_t value) {
            seed ^= value + static_cast<std::size_t>(0x9e3779b9U) +
                    (seed << 6U) + (seed >> 2U);
        };

        mix(std::hash<bool>{}(key.virtual_token.has_value()));
        if (key.virtual_token) {
            mix(std::hash<VirtualSemanticItemToken>{}(*key.virtual_token));
        }
        return seed;
    }
};

} // namespace semantic_proxy_detail

/// Per-native-view weak cache for platform semantic proxy objects.
///
/// The cache owns no semantic content and never keeps a native proxy alive on
/// behalf of a removed semantic node.  Native readers may call this from their
/// platform callback threads, so cache bookkeeping is synchronized.  Factory
/// construction is intentionally serialized under the cache lock: the factory
/// is an internal platform-proxy constructor and must not re-enter this cache;
/// no application callback is invoked while the lock is held.  This guarantees
/// that concurrent OS queries for one semantic identity observe one canonical
/// live native proxy object.
template <class NativeProxy>
class SemanticProxyCache {
public:
    explicit SemanticProxyCache(
        const std::shared_ptr<SemanticSnapshotPublisher>& publisher) noexcept
        : publisher_(publisher) {}

    SemanticProxyCache(const SemanticProxyCache&) = delete;
    SemanticProxyCache& operator=(const SemanticProxyCache&) = delete;

    template <class Factory>
    [[nodiscard]] std::shared_ptr<NativeProxy> ordinary(SemanticId node_id,
                                                        Factory&& factory) {
        return get_or_create(
            semantic_proxy_detail::CacheKey{node_id, std::nullopt},
            std::forward<Factory>(factory));
    }

    template <class Factory>
    [[nodiscard]] std::shared_ptr<NativeProxy> virtual_item(
        SemanticId list_node_id,
        VirtualSemanticItemToken token,
        Factory&& factory) {
        return get_or_create(
            semantic_proxy_detail::CacheKey{list_node_id, token},
            std::forward<Factory>(factory));
    }

    [[nodiscard]] std::size_t tracked_identities() const {
        std::lock_guard lock{mutex_};
        return entries_.size();
    }

    std::size_t prune_expired() {
        std::lock_guard lock{mutex_};
        std::size_t erased = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expired()) {
                it = entries_.erase(it);
                ++erased;
            } else {
                ++it;
            }
        }
        return erased;
    }

private:
    template <class Factory>
    [[nodiscard]] std::shared_ptr<NativeProxy> get_or_create(
        const semantic_proxy_detail::CacheKey& key,
        Factory&& factory) {
        auto publisher = publisher_.lock();
        if (!publisher) {
            return {};
        }

        std::lock_guard lock{mutex_};
        auto entry = entries_.find(key);
        if (entry != entries_.end()) {
            if (auto existing = entry->second.lock()) {
                return existing;
            }
        }

        auto source = key.virtual_token
            ? SemanticSnapshotProxy::virtual_item(publisher, key.node_id, *key.virtual_token)
            : SemanticSnapshotProxy::ordinary(publisher, key.node_id);

        // Do not resurrect an identity that is absent from the latest
        // immutable semantic generation. Existing live native proxies remain
        // usable as defunct objects and report absence through source.read().
        if (!source.read()) {
            if (entry != entries_.end()) {
                entries_.erase(entry);
            }
            return {};
        }

        auto created = std::forward<Factory>(factory)(std::move(source));
        if (!created) {
            if (entry != entries_.end()) {
                entries_.erase(entry);
            }
            return {};
        }

        entries_[key] = created;
        return created;
    }

    std::weak_ptr<SemanticSnapshotPublisher> publisher_;
    mutable std::mutex mutex_;
    std::unordered_map<semantic_proxy_detail::CacheKey,
                       std::weak_ptr<NativeProxy>,
                       semantic_proxy_detail::CacheKeyHash>
        entries_;
};

} // namespace ui::detail
