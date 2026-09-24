#pragma once

#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_rules.hpp>
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

/// Internal widget-side semantic action seam.
///
/// T045 deliberately exposes only Component::semantics() as the public custom
/// component accessibility hook. Built-in widgets that advertise mutating or
/// activation actions implement this private detail interface so the UI-thread
/// live-tree resolver can invoke their normal state/callback policy directly,
/// without manufacturing pointer/key events or adding a second public semantic
/// callback API.
class SemanticActionHandler {
public:
    virtual ~SemanticActionHandler() = default;

    [[nodiscard]] virtual bool perform_semantic_action(
        const SemanticActionRequest& request) = 0;
};

/// Internal logical-item action seam for virtual semantic collections.
///
/// The token is the stable T067 logical identity, never a materialized row
/// index or Component pointer. Implementations must resolve it against their
/// current dataset at call time and fail closed when it is stale. This keeps
/// native accessibility actions independent from the visual virtualization
/// window and preserves the same callback/lifetime rules as normal widgets.
class VirtualSemanticActionHandler {
public:
    virtual ~VirtualSemanticActionHandler() = default;

    [[nodiscard]] virtual bool perform_virtual_semantic_action(
        VirtualSemanticItemToken token,
        const SemanticActionRequest& request) = 0;
};

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
/// teardown turns queued work into a safe no-op. Every router must carry the
/// owning view's independent weak endpoint token: there is intentionally no
/// unguarded constructor that can bypass teardown invalidation. This lets
/// teardown reject nested/future posts immediately even while one in-flight
/// dispatcher callback temporarily keeps the target object itself alive.
class SemanticActionRouter final {
public:
    SemanticActionRouter(SemanticSnapshotProxy proxy,
                         Dispatcher dispatcher,
                         const std::shared_ptr<SemanticActionTarget>& target,
                         std::weak_ptr<const void> endpoint_lifetime)
        : proxy_(std::move(proxy)),
          dispatcher_(std::move(dispatcher)),
          target_(target),
          endpoint_lifetime_(std::move(endpoint_lifetime)) {}

    [[nodiscard]] bool post(SemanticActionRequest request) const noexcept {
        // This entry point is called from platform accessibility callbacks. A
        // snapshot materialization or Dispatcher queue allocation may throw;
        // fail closed instead of allowing C++ exceptions to cross a foreign
        // callback ABI. Rejection leaves the live tree untouched.
        try {
            if (endpoint_lifetime_.expired()) {
                return false;
            }

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
            auto endpoint_lifetime = endpoint_lifetime_;

            return dispatcher_.post(
                [proxy = std::move(proxy),
                 target = std::move(target),
                 endpoint_lifetime = std::move(endpoint_lifetime),
                 identity,
                 request = std::move(request)]() mutable {
                    if (endpoint_lifetime.expired()) {
                        return;
                    }

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

                    // current_semantics() may invoke custom component code. A
                    // re-entrant owner teardown must become visible before the
                    // mutating action is allowed to begin, even though
                    // live_target itself remains strongly held in this frame.
                    if (endpoint_lifetime.expired()) {
                        return;
                    }

                    (void)live_target->dispatch_semantic_action(identity, request);
                });
        } catch (...) {
            return false;
        }
    }

private:
    SemanticSnapshotProxy proxy_;
    Dispatcher dispatcher_;
    std::weak_ptr<SemanticActionTarget> target_;
    std::weak_ptr<const void> endpoint_lifetime_;
};

} // namespace ui::detail