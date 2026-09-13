#pragma once

#include <nativeui/detail/semantic_snapshot.hpp>

#include <memory>
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

    [[nodiscard]] std::vector<SemanticChange> publish(SemanticTreeSnapshot candidate) {
        return publisher_->publish(std::move(candidate));
    }

private:
    std::shared_ptr<SemanticSnapshotPublisher> publisher_;
};

} // namespace ui::detail
