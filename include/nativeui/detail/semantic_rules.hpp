#pragma once

#include <nativeui/semantics.hpp>

#include <algorithm>
#include <utility>

namespace ui::detail {

/// Classify whether a semantic action mutates the target's application value.
///
/// T045/T059 use this one closed-set policy at both publication time and the
/// T068 execution-time recheck. Keeping the classification in one helper avoids
/// allowing a read-only action through one boundary while filtering it at the
/// other.
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

/// Apply retained effective availability/focus state to a component-provided
/// semantic value before it is published in an immutable snapshot.
///
/// T059 availability remains authoritative: effective Disabled removes focus
/// eligibility and all actions while preserving descriptive/value state;
/// effective ReadOnly keeps navigation/focus behavior but removes semantic
/// actions that mutate application values. The tree-provided focus flag is
/// authoritative and is only exposed for an enabled, focusable node.
[[nodiscard]] inline SemanticInfo normalize_semantic_info(
    SemanticInfo info,
    bool effective_enabled,
    bool effective_read_only,
    bool tree_focused) {
    info.enabled = info.enabled && effective_enabled;
    info.read_only = info.read_only || effective_read_only;

    if (!info.enabled) {
        info.focusable = false;
        info.focused = false;
        info.actions.clear();
        return info;
    }

    info.focused = info.focusable && tree_focused;

    std::erase_if(info.actions, [&](SemanticAction action) {
        if (action == SemanticAction::Focus && !info.focusable) {
            return true;
        }
        return info.read_only && semantic_action_mutates_value(action);
    });

    return info;
}

} // namespace ui::detail
