#pragma once

#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/semantics.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>

namespace ui::detail {

/// Resolve one current logical ListView item from the immutable virtual-list token
/// index without consulting or materializing visual rows.
///
/// Virtual ListView action dispatch requires the indexed metadata form. A
/// missing index therefore fails closed rather than falling back to an O(N)
/// scan on an accessibility callback path.
[[nodiscard]] inline std::optional<std::size_t> virtual_list_semantic_index_for_token(
    const VirtualSemanticChildren& children,
    VirtualSemanticItemToken token) {
    if (token == kInvalidVirtualSemanticItemToken) return std::nullopt;

    const auto& token_index = children.token_index_snapshot();
    const auto& metadata = children.metadata_snapshot();
    if (!token_index || !metadata) return std::nullopt;

    const auto found = token_index->find(token);
    if (found == token_index->end() || found->second >= metadata->size() ||
        (*metadata)[found->second].token != token) {
        return std::nullopt;
    }
    return found->second;
}

[[nodiscard]] inline bool virtual_list_semantic_action_allowed(
    const VirtualSemanticChildren& children,
    std::size_t index,
    SemanticAction action) {
    const auto& metadata = children.metadata_snapshot();
    if (!metadata || index >= metadata->size()) return false;

    const auto& item = (*metadata)[index];
    if (!item.enabled ||
        std::find(item.actions.begin(), item.actions.end(), action) == item.actions.end()) {
        return false;
    }
    return !item.read_only || !semantic_action_mutates_value(action);
}

/// Execute one logical-item semantic action for a retained virtual-list ListView.
///
/// The live-tree resolver has already rechecked effective container
/// availability immediately before entering this helper. We still resolve the
/// token against the runtime's current immutable dataset projection and apply
/// the item's current action/Disabled/ReadOnly policy before invoking the normal
/// retained selection/activation path. The metadata/index lookup is
/// allocation-free and the runtime alone owns any scrolling/materialization;
/// semantic lookup never calls the visual row factory.
///
/// `SemanticAction::Focus` needs the owning tree's focus manager as well as the
/// logical row index, so it is dispatched through `request_owner_focus`. The
/// owner callback must move retained keyboard focus to the composite ListView
/// and report success; only then does the normal virtual-list scroll/materialization
/// window reveal the logical item. Focus is non-mutating: selection is left
/// untouched. Stale tokens, disabled/read-only items, unadvertised actions and a
/// rejected owner focus request all fail closed without touching the visual
/// window. The default overload without an owner fails Focus closed.
template <class Runtime>
[[nodiscard]] bool dispatch_virtual_list_semantic_action(
    Runtime& runtime,
    VirtualSemanticItemToken token,
    const SemanticActionRequest& request,
    const std::function<bool()>& request_owner_focus) {
    const auto children = runtime.semantic_children(Rect{});
    const auto index = virtual_list_semantic_index_for_token(children, token);
    if (!index) return false;

    if (request.action == SemanticAction::Focus) {
        if (!virtual_list_semantic_action_allowed(children, *index, request.action)) {
            return false;
        }

        // Retained keyboard focus moves to the composite owner first. A denied
        // owner leaves the visual window untouched; an accepted owner is
        // followed by the ordinary virtual-list reveal so the offscreen logical item
        // becomes the focused exception row without a second semantic lookup.
        if (!request_owner_focus || !request_owner_focus()) return false;
        return runtime.focus_index(*index);
    }

    if (request.action != SemanticAction::Select &&
        request.action != SemanticAction::Activate) {
        return false;
    }
    if (!virtual_list_semantic_action_allowed(children, *index, request.action)) {
        return false;
    }

    // select()/activate() may synchronously publish state and re-enter
    // application code. Return directly and do not access the semantic
    // snapshot/runtime again after either call begins.
    switch (request.action) {
    case SemanticAction::Select:
        return runtime.select(*index);
    case SemanticAction::Activate:
        return runtime.activate(*index);
    default:
        return false;
    }
}

template <class Runtime>
[[nodiscard]] bool dispatch_virtual_list_semantic_action(
    Runtime& runtime,
    VirtualSemanticItemToken token,
    const SemanticActionRequest& request) {
    return dispatch_virtual_list_semantic_action(
        runtime, token, request, std::function<bool()>{});
}

} // namespace ui::detail
