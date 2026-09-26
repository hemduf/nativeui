#pragma once

#include <nativeui/detail/semantic_snapshot.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui::detail {

/// One completed per-view semantic publication checkpoint.
///
/// Native platform adapters retain the exact immutable snapshot that produced
/// `changes` instead of publishing notifications from a later `current()` read.
/// The snapshot may be the same shared object as the previous generation when a
/// staged candidate exposed no semantic change; the presence of this batch still
/// records that a pending checkpoint was consumed.
struct SemanticPublicationBatch final {
    std::shared_ptr<const SemanticTreeSnapshot> snapshot;
    std::vector<SemanticChange> changes;

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return snapshot ? snapshot->generation : 0;
    }
};

/// Per-native-view owner for immutable accessibility snapshot publication.
///
/// Native bridges keep only weak SemanticSnapshotProxy references to this
/// owner. Destroying one view therefore makes all of that view's proxies
/// defunct without affecting sibling views, while readers that already retained
/// an immutable generation remain valid through their shared snapshot handle.
class SemanticViewState final {
public:
    SemanticViewState()
        : publisher_(std::make_shared<SemanticSnapshotPublisher>()) {}

    SemanticViewState(const SemanticViewState&) = delete;
    SemanticViewState& operator=(const SemanticViewState&) = delete;
    SemanticViewState(SemanticViewState&&) = delete;
    SemanticViewState& operator=(SemanticViewState&&) = delete;

    [[nodiscard]] bool active() const noexcept {
        return static_cast<bool>(publisher_);
    }

    [[nodiscard]] std::shared_ptr<SemanticSnapshotPublisher> publisher() const noexcept {
        return publisher_;
    }

    [[nodiscard]] std::shared_ptr<const SemanticTreeSnapshot> current() const noexcept {
        return publisher_ ? publisher_->current() : nullptr;
    }

    /// Permanently retire this view's semantic publication domain. Query proxies
    /// hold only weak publisher references, while publisher shutdown also makes
    /// any accidentally retained strong publisher reference observe a defunct
    /// root. A reader that already retained one immutable generation stays valid
    /// until that read releases its snapshot. Pending unpublished state is
    /// discarded because a closed native view must never publish or resurrect
    /// semantic state later.
    void shutdown() noexcept {
        pending_.reset();
        if (publisher_) publisher_->shutdown();
        publisher_.reset();
    }

    /// Stage the latest UI-thread semantic candidate for the next publication
    /// checkpoint. Repeated state/layout changes before that checkpoint replace
    /// the pending value instead of manufacturing intermediate native
    /// notifications/generations. The retained candidate owns only immutable
    /// semantic values/shared T067 metadata, never live tree pointers.
    void stage(SemanticTreeSnapshot candidate) {
        if (!publisher_) return;
        pending_ = std::move(candidate);
    }

    [[nodiscard]] bool has_pending_publication() const noexcept {
        return publisher_ && pending_.has_value();
    }

    /// Publish at most one resulting generation for all UI changes staged since
    /// the previous checkpoint and return that exact immutable generation with
    /// its closed-set notification batch. No pending candidate is represented by
    /// nullopt; a consumed candidate with no exposed change returns a batch with
    /// an empty `changes` vector and the unchanged current generation.
    ///
    /// The authoritative pending candidate is retained until publication has
    /// completed successfully. Copying the small snapshot shell does not copy a
    /// virtual ListView's O(N) T067 metadata because those entries remain behind
    /// shared immutable storage. If snapshot copying, diff preparation or final
    /// snapshot allocation throws, the current published generation is unchanged
    /// and the same pending candidate remains available for a later checkpoint.
    [[nodiscard]] std::optional<SemanticPublicationBatch> checkpoint_publication() {
        if (!publisher_ || !pending_) {
            return std::nullopt;
        }

        auto candidate = *pending_;
        auto changes = publisher_->publish(std::move(candidate));
        auto snapshot = publisher_->current();
        if (!snapshot) {
            return std::nullopt;
        }

        pending_.reset();
        return SemanticPublicationBatch{
            std::move(snapshot),
            std::move(changes),
        };
    }

    /// Compatibility seam for callers interested only in notification
    /// categories. Platform adapters should prefer checkpoint_publication() so
    /// notifications remain bound to the exact immutable generation.
    [[nodiscard]] std::vector<SemanticChange> checkpoint() {
        auto publication = checkpoint_publication();
        return publication ? std::move(publication->changes)
                           : std::vector<SemanticChange>{};
    }

    [[nodiscard]] std::optional<SemanticPublicationBatch> publish_publication(
        SemanticTreeSnapshot candidate) {
        if (!publisher_) return std::nullopt;

        // An explicit immediate publication supersedes any older staged UI
        // checkpoint value. Route it through the same durable pending state as a
        // normal checkpoint so a fallible publication cannot silently discard
        // the newest semantic state.
        pending_ = std::move(candidate);
        return checkpoint_publication();
    }

    [[nodiscard]] std::vector<SemanticChange> publish(SemanticTreeSnapshot candidate) {
        auto publication = publish_publication(std::move(candidate));
        return publication ? std::move(publication->changes)
                           : std::vector<SemanticChange>{};
    }

private:
    std::shared_ptr<SemanticSnapshotPublisher> publisher_;
    std::optional<SemanticTreeSnapshot> pending_;
};

} // namespace ui::detail
