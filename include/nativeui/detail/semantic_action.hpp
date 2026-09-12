#pragma once

#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/dispatcher.hpp>
#include <nativeui/semantics.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ui::detail {

struct SemanticIdentity {
    SemanticId node_id{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token;

    bool operator==(const SemanticIdentity&) const noexcept = default;
};

struct SemanticActionRequest {
    SemanticAction action{SemanticAction::Activate};
    std::optional<double> numeric_value;
    std::optional<std::string> text_value;
};

[[nodiscard]] inline bool semantic_action_mutates_value(SemanticAction action) noexcept {
    switch (action) {
        case SemanticAction::Toggle:
        case SemanticAction::Increment:
        case SemanticAction::Decrement:
        case SemanticAction::SetValue:
        case SemanticAction::Select:
            return true;
        case SemanticAction::Activate:
        case SemanticAction::Focus:
        case SemanticAction::Expand:
        case SemanticAction::Collapse:
            return false;
    }
    return true;
}

[[nodiscard]] inline bool semantic_action_allowed(
    const SemanticInfo& info,
    SemanticAction action) noexcept {
    if (!info.enabled || !info.supports(action)) {
        return false;
    }
    return !info.read_only || !semantic_action_mutates_value(action);
}

/// UI-thread endpoint for semantic actions after snapshot validation.
///
/// Implementations resolve the stable semantic identity against the current
/// live retained state; they never retain raw Node/Component pointers across
/// dispatcher checkpoints. `current_semantics()` is intentionally value based
/// so the router can perform the mandatory current-state eligibility recheck
/// before allowing a mutation/action to reach the concrete component policy.
class SemanticActionTarget {
public:
    virtual ~SemanticActionTarget() = default;

    [[nodiscard]] virtual std::optional<SemanticInfo> current_semantics(
        const SemanticIdentity& identity) const = 0;

    virtual bool dispatch_semantic_action(
        const SemanticIdentity& identity,
        const SemanticActionRequest& request) = 0;
};

/// Thread-safe request boundary from native accessibility callbacks to the
/// owning NativeUI UI thread.
///
/// Native readers first validate against the current immutable semantic
/// snapshot. Accepted requests are posted through T065 and, at execution, are
/// validated again against both the latest published snapshot and current live
/// semantics supplied by the owning view. The target is weakly held so view
/// teardown turns queued work into a safe no-op.
class SemanticActionRouter final {
public:
    SemanticActionRouter(SemanticSnapshotProxy proxy,
                         Dispatcher dispatcher,
                         const std::shared_ptr<SemanticActionTarget>& target)
        : proxy_(std::move(proxy)),
          dispatcher_(std::move(dispatcher)),
          target_(target) {}

    [[nodiscard]] bool post(SemanticActionRequest request) const {
        const auto advertised = proxy_.read();
        if (!advertised ||
            !semantic_action_allowed(advertised->info(), request.action) ||
            target_.expired()) {
            return false;
        }

        const SemanticIdentity identity{
            advertised->node_id(), advertised->virtual_token()};
        auto proxy = proxy_;
        auto target = target_;

        return dispatcher_.post(
            [proxy = std::move(proxy),
             target = std::move(target),
             identity,
             request = std::move(request)]() mutable {
                const auto published = proxy.read();
                if (!published) {
                    return;
                }

                const SemanticIdentity published_identity{
                    published->node_id(), published->virtual_token()};
                if (published_identity != identity ||
                    !semantic_action_allowed(published->info(), request.action)) {
                    return;
                }

                const auto live_target = target.lock();
                if (!live_target) {
                    return;
                }

                const auto live_info = live_target->current_semantics(identity);
                if (!live_info || !semantic_action_allowed(*live_info, request.action)) {
                    return;
                }

                (void)live_target->dispatch_semantic_action(identity, request);
            });
    }

private:
    SemanticSnapshotProxy proxy_;
    Dispatcher dispatcher_;
    std::weak_ptr<SemanticActionTarget> target_;
};

} // namespace ui::detail
