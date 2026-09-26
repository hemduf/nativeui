#pragma once

#include <nativeui/detail/semantic_action.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ui::detail {

/// UI-thread semantic-action target bound to one owning view/root lifetime.
///
/// Native/platform code retains this target, not a Tree/Node/Component pointer.
/// The supplied resolver callbacks may borrow the owning retained state only for
/// the duration of one UI-thread call. The owner must reset its lifetime token
/// before retained-tree teardown begins.
///
/// Deliberately do not lock the weak lifetime token into temporary shared
/// ownership. Keeping that token alive across a callback would make nested
/// semantic work incorrectly observe a view as alive after a re-entrant owner
/// teardown. Target methods are UI-thread-only; T065 is responsible for
/// marshalling native actions onto that thread before invoking this object.
class LifetimeBoundSemanticActionTarget final : public SemanticActionTarget {
public:
    using CurrentSemanticsResolver =
        std::function<std::optional<SemanticInfo>(const SemanticIdentity&)>;
    using ActionDispatcher =
        std::function<bool(const SemanticIdentity&, const SemanticActionRequest&)>;

    LifetimeBoundSemanticActionTarget(
        std::weak_ptr<const void> owner_lifetime,
        CurrentSemanticsResolver current_semantics,
        ActionDispatcher dispatch_action)
        : owner_lifetime_(std::move(owner_lifetime)),
          current_semantics_(std::move(current_semantics)),
          dispatch_action_(std::move(dispatch_action)) {}

    [[nodiscard]] std::optional<SemanticInfo> current_semantics(
        const SemanticIdentity& identity) const override {
        if (owner_lifetime_.expired() || !current_semantics_) {
            return std::nullopt;
        }

        // The resolver may reach component code. Return its value directly and
        // do not inspect owner-backed state after it begins.
        return current_semantics_(identity);
    }

    [[nodiscard]] bool dispatch_semantic_action(
        const SemanticIdentity& identity,
        const SemanticActionRequest& request) override {
        if (owner_lifetime_.expired() || !dispatch_action_) {
            return false;
        }

        // The action may synchronously remove a subtree or tear down the owning
        // view. Return directly so no owner-backed state is touched afterwards.
        return dispatch_action_(identity, request);
    }

private:
    std::weak_ptr<const void> owner_lifetime_;
    CurrentSemanticsResolver current_semantics_;
    ActionDispatcher dispatch_action_;
};

} // namespace ui::detail
