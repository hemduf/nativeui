#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_rules.hpp>
#include <nativeui/detail/semantic_virtual_source.hpp>

#include <functional>
#include <optional>

namespace ui::detail {

namespace semantic_live_action_detail {

[[nodiscard]] inline const Node* find_node(
    const Node& root, SemanticId id) noexcept {
    if (static_cast<SemanticId>(root.id) == id) {
        return &root;
    }
    for (const auto& child : root.children) {
        if (!child) continue;
        if (const auto* found = find_node(static_cast<const Node&>(*child), id)) {
            return found;
        }
    }
    return nullptr;
}

[[nodiscard]] inline Node* find_node(Node& root, SemanticId id) noexcept {
    if (static_cast<SemanticId>(root.id) == id) {
        return &root;
    }
    for (auto& child : root.children) {
        if (!child) continue;
        if (auto* found = find_node(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace semantic_live_action_detail

/// Resolve current ordinary-node or virtual-item semantics from the live
/// retained state.
///
/// This is a UI-thread-only execution helper for SemanticActionTarget
/// implementations. The root reference is borrowed for this call only and is
/// never retained by a proxy, snapshot or queued request. Virtual identities
/// re-resolve the owning semantic container and query its current immutable
/// logical-item projection; they never require a materialized visual row.
[[nodiscard]] inline std::optional<SemanticInfo> current_live_semantics(
    const Node& root,
    const SemanticIdentity& identity,
    NodeId focused_node_id = kInvalidNodeId) {
    if (identity.node_id == kInvalidSemanticId) {
        return std::nullopt;
    }

    const auto* node = semantic_live_action_detail::find_node(root, identity.node_id);
    if (!node || !node->component) {
        return std::nullopt;
    }

    const auto availability = node->component->effective_availability();
    if (availability.visibility != VisibilityMode::Visible) {
        return std::nullopt;
    }

    auto container_info = normalize_semantic_info(
        node->component->semantics(),
        availability.enabled,
        availability.read_only,
        node->id == focused_node_id);
    if (container_info.role == SemanticRole::None) {
        return std::nullopt;
    }

    if (!identity.virtual_token) {
        return container_info;
    }
    if (*identity.virtual_token == kInvalidVirtualSemanticItemToken) {
        return std::nullopt;
    }

    const auto* virtual_source =
        dynamic_cast<const VirtualSemanticChildrenSource*>(node->component.get());
    if (!virtual_source) {
        return std::nullopt;
    }

    const auto children = virtual_source->virtual_semantic_children(node->bounds);
    const auto item = resolve_indexed_virtual_semantic_item(
        children, *identity.virtual_token);
    if (!item) {
        return std::nullopt;
    }

    auto item_info = normalize_semantic_info(
        item->info,
        container_info.enabled,
        container_info.read_only,
        false);
    if (item_info.role == SemanticRole::None) {
        return std::nullopt;
    }
    return item_info;
}

/// UI-thread focus requester supplied by the owning retained tree. It resolves
/// one exposed semantic node to the nearest retained focusable and fails closed
/// when the tree has no active platform focus domain.
using SemanticFocusRequester = std::function<bool(SemanticId)>;

/// Re-resolve and execute one built-in semantic action on the UI thread.
/// Eligibility is recomputed from the current component semantics and effective
/// availability immediately before dispatch. No Node/Component pointer survives
/// this call and no retained-tree state is accessed after a handler begins, so
/// a normal widget callback may synchronously remove its subtree according to
/// the existing widget action policy.
///
/// `request_focus` is supplied by the owning retained tree. `SemanticAction::Focus`
/// is owned by the retained focus manager, not by widget value/state handlers:
/// ordinary identities request focus for the exposed node (resolved by the tree
/// to the nearest retained focusable, so composite widgets keep one keyboard Tab
/// stop) and virtual identities delegate to `VirtualSemanticActionHandler` so
/// the logical item can be revealed through normal T067 scroll/materialization
/// after the owner accepted focus. A missing requester therefore fails Focus
/// closed.
///
/// Virtual identities are resolved through the owning logical collection using
/// their stable T067 token and are dispatched only to VirtualSemanticActionHandler.
/// They are never redirected to a currently materialized visual row. The live
/// action path requires an immutable token index so resolution cannot silently
/// degrade to O(N) for very large virtual collections.
[[nodiscard]] inline bool dispatch_live_semantic_action(
    Node& root,
    const SemanticIdentity& identity,
    const SemanticActionRequest& request,
    NodeId focused_node_id = kInvalidNodeId,
    const SemanticFocusRequester& request_focus = {}) {
    if (identity.node_id == kInvalidSemanticId) {
        return false;
    }

    auto* node = semantic_live_action_detail::find_node(root, identity.node_id);
    if (!node || !node->component) {
        return false;
    }

    const auto availability = node->component->effective_availability();
    if (availability.visibility != VisibilityMode::Visible) {
        return false;
    }

    const auto container_info = normalize_semantic_info(
        node->component->semantics(),
        availability.enabled,
        availability.read_only,
        node->id == focused_node_id);
    if (container_info.role == SemanticRole::None) {
        return false;
    }

    if (identity.virtual_token) {
        const auto token = *identity.virtual_token;
        if (token == kInvalidVirtualSemanticItemToken) {
            return false;
        }

        auto* virtual_source =
            dynamic_cast<VirtualSemanticChildrenSource*>(node->component.get());
        if (!virtual_source) {
            return false;
        }

        const auto children = virtual_source->virtual_semantic_children(node->bounds);
        const auto item = resolve_indexed_virtual_semantic_item(children, token);
        if (!item) {
            return false;
        }

        const auto item_info = normalize_semantic_info(
            item->info,
            container_info.enabled,
            container_info.read_only,
            false);
        if (item_info.role == SemanticRole::None ||
            !semantic_action_allowed(item_info, request.action)) {
            return false;
        }

        auto* handler =
            dynamic_cast<VirtualSemanticActionHandler*>(node->component.get());
        if (!handler) {
            return false;
        }

        // The handler may invoke application code that removes/destroys this
        // node. The token and request are values; do not inspect any live-tree
        // object after the call begins.
        return handler->perform_virtual_semantic_action(token, request);
    }

    if (!semantic_action_allowed(container_info, request.action)) {
        return false;
    }

    if (request.action == SemanticAction::Focus) {
        // Keyboard focus belongs to the retained focus manager. Widgets never
        // implement Focus as a value/state mutation, so the tree resolves the
        // exposed identity to its nearest retained focusable owner (composite
        // widgets expose semantic children that remain one keyboard Tab stop).
        return request_focus && request_focus(static_cast<SemanticId>(node->id));
    }

    auto* handler = dynamic_cast<SemanticActionHandler*>(node->component.get());
    if (!handler) {
        return false;
    }

    // The handler may invoke application code that removes/destroys this node.
    // Do not inspect node/component/handler after the call begins.
    return handler->perform_semantic_action(request);
}

} // namespace ui::detail
