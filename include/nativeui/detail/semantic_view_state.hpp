#pragma once

#include <nativeui/detail/semantic_snapshot.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui::detail {

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

    [[nodiscard]] std::shared_ptr<SemanticSnapshotPublisher> publisher() const noexcept {
        return publisher_;
    }

    [[nodiscard]] std::shared_ptr<const SemanticTreeSnapshot> current() const noexcept {
        return publisher_->current();
    }

    /// Stage the latest UI-thread semantic candidate for the next publication
    /// checkpoint. Repeated state/layout changes before that checkpoint replace
    /// the pending value instead of manufacturing intermediate native
    /// notifications/generations. The retained candidate owns only immutable
    /// semantic values/shared T067 metadata, never live tree pointers.
    void stage(SemanticTreeSnapshot candidate) {
        pending_ = std::move(candidate);
    }

    [[nodiscard]] bool has_pending_publication() const noexcept {
        return pending_.has_value();
    }

    /// Publish at most one resulting generation for all UI changes staged since
    /// the previous checkpoint and return the closed-set notification batch for
    /// that final state. No pending candidate is a no-op.
    [[nodiscard]] std::vector<SemanticChange> checkpoint() {
        if (!pending_) {
            return {};
        }

        auto candidate = std::move(*pending_);
        pending_.reset();
        return publisher_->publish(std::move(candidate));
    }

    [[nodiscard]] std::vector<SemanticChange> publish(SemanticTreeSnapshot candidate) {
        // An explicit immediate publication supersedes any older staged UI
        // checkpoint value rather than allowing it to publish later out of
        // order.
        pending_.reset();
        return publisher_->publish(std::move(candidate));
    }

private:
    std::shared_ptr<SemanticSnapshotPublisher> publisher_;
    std::optional<SemanticTreeSnapshot> pending_;
};

} // namespace ui::detail
