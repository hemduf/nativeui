#pragma once

#include <nativeui/semantics.hpp>

#include <unordered_map>
#include <vector>

namespace ui::detail {

namespace semantic_snapshot_detail {

[[nodiscard]] inline bool rect_equal(const Rect& lhs, const Rect& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
}

[[nodiscard]] inline bool value_fields_equal(const SemanticInfo& lhs,
                                             const SemanticInfo& rhs) {
    return lhs.role == rhs.role && lhs.name == rhs.name &&
           lhs.description == rhs.description && lhs.text_value == rhs.text_value &&
           lhs.numeric_value == rhs.numeric_value && lhs.value_range == rhs.value_range &&
           lhs.enabled == rhs.enabled && lhs.read_only == rhs.read_only &&
           lhs.checked == rhs.checked && lhs.expanded == rhs.expanded &&
           lhs.focusable == rhs.focusable && lhs.actions == rhs.actions;
}

[[nodiscard]] inline bool virtual_structure_equal(const VirtualSemanticChildren& lhs,
                                                   const VirtualSemanticChildren& rhs) noexcept {
    if (lhs.dataset_generation() != rhs.dataset_generation() || lhs.size() != rhs.size()) {
        return false;
    }

    const auto& lhs_metadata = lhs.metadata_snapshot();
    const auto& rhs_metadata = rhs.metadata_snapshot();
    if (lhs_metadata.get() == rhs_metadata.get()) {
        return true;
    }
    if (!lhs_metadata || !rhs_metadata || lhs_metadata->size() != rhs_metadata->size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs_metadata->size(); ++index) {
        if ((*lhs_metadata)[index].token != (*rhs_metadata)[index].token) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool virtual_value_equal(const VirtualSemanticChildren& lhs,
                                               const VirtualSemanticChildren& rhs) {
    const auto& lhs_metadata = lhs.metadata_snapshot();
    const auto& rhs_metadata = rhs.metadata_snapshot();
    if (lhs_metadata.get() == rhs_metadata.get()) {
        return true;
    }
    if (!lhs_metadata || !rhs_metadata || lhs_metadata->size() != rhs_metadata->size()) {
        return false;
    }
    return *lhs_metadata == *rhs_metadata;
}

} // namespace semantic_snapshot_detail

[[nodiscard]] inline std::vector<SemanticChange> diff_semantic_snapshots(
    const SemanticTreeSnapshot& before,
    const SemanticTreeSnapshot& after) {
    bool structure_changed = before.root != after.root || before.nodes.size() != after.nodes.size();
    bool focus_changed = false;
    bool selection_changed = false;
    bool value_changed = false;
    bool bounds_changed = false;

    std::unordered_map<SemanticId, const SemanticNodeSnapshot*> before_by_id;
    before_by_id.reserve(before.nodes.size());
    for (const auto& node : before.nodes) {
        before_by_id.emplace(node.id, &node);
    }

    std::unordered_map<SemanticId, const SemanticNodeSnapshot*> after_by_id;
    after_by_id.reserve(after.nodes.size());
    for (const auto& node : after.nodes) {
        after_by_id.emplace(node.id, &node);
    }

    if (before_by_id.size() != before.nodes.size() || after_by_id.size() != after.nodes.size()) {
        structure_changed = true;
    }

    for (const auto& after_node : after.nodes) {
        const auto before_it = before_by_id.find(after_node.id);
        if (before_it == before_by_id.end()) {
            structure_changed = true;
            continue;
        }

        const auto& before_node = *before_it->second;
        if (before_node.parent != after_node.parent || before_node.children != after_node.children) {
            structure_changed = true;
        }

        if (before_node.virtual_children.has_value() != after_node.virtual_children.has_value()) {
            structure_changed = true;
        } else if (before_node.virtual_children && after_node.virtual_children) {
            if (!semantic_snapshot_detail::virtual_structure_equal(
                    *before_node.virtual_children, *after_node.virtual_children)) {
                structure_changed = true;
            } else if (!semantic_snapshot_detail::virtual_value_equal(
                           *before_node.virtual_children, *after_node.virtual_children)) {
                value_changed = true;
            }
        }

        focus_changed = focus_changed || before_node.info.focused != after_node.info.focused;
        selection_changed =
            selection_changed || before_node.info.selected != after_node.info.selected;
        value_changed = value_changed ||
                        !semantic_snapshot_detail::value_fields_equal(before_node.info,
                                                                     after_node.info);
        bounds_changed = bounds_changed ||
                         !semantic_snapshot_detail::rect_equal(before_node.bounds,
                                                               after_node.bounds);
    }

    if (!structure_changed) {
        for (const auto& before_node : before.nodes) {
            if (!after_by_id.contains(before_node.id)) {
                structure_changed = true;
                break;
            }
        }
    }

    std::vector<SemanticChange> changes;
    changes.reserve(5);
    if (structure_changed) {
        changes.push_back(SemanticChange::StructureChanged);
    }
    if (focus_changed) {
        changes.push_back(SemanticChange::FocusChanged);
    }
    if (selection_changed) {
        changes.push_back(SemanticChange::SelectionChanged);
    }
    if (value_changed) {
        changes.push_back(SemanticChange::ValueChanged);
    }
    if (bounds_changed) {
        changes.push_back(SemanticChange::BoundsChanged);
    }
    return changes;
}

} // namespace ui::detail
