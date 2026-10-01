#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/semantic_rules.hpp>
#include <nativeui/detail/semantic_virtual_source.hpp>
#include <nativeui/semantics.hpp>

#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

namespace semantic_tree_detail {

struct InheritedSemanticFields {
    std::string description;
};

[[nodiscard]] inline std::vector<SemanticId> append_semantic_nodes(
    const Node& source,
    SemanticId semantic_parent,
    const InheritedSemanticFields& inherited,
    NodeId focused_node_id,
    SemanticTreeSnapshot& snapshot) {
    if (!source.component) {
        std::vector<SemanticId> exposed;
        for (const auto& child : source.children) {
            if (!child) continue;
            auto child_ids = append_semantic_nodes(
                *child, semantic_parent, inherited, focused_node_id, snapshot);
            exposed.insert(exposed.end(), child_ids.begin(), child_ids.end());
        }
        return exposed;
    }

    const auto availability = source.component->effective_availability();
    if (availability.visibility != VisibilityMode::Visible) {
        return {};
    }

    auto info = source.component->semantics();
    if (info.description.empty() && !inherited.description.empty()) {
        info.description = inherited.description;
    }

    info = normalize_semantic_info(
        std::move(info),
        availability.enabled,
        availability.read_only,
        source.id == focused_node_id);

    if (info.role == SemanticRole::None) {
        auto next_inherited = inherited;
        if (!info.description.empty()) {
            next_inherited.description = info.description;
        }

        std::vector<SemanticId> exposed;
        for (const auto& child : source.children) {
            if (!child) continue;
            auto child_ids = append_semantic_nodes(
                *child, semantic_parent, next_inherited, focused_node_id, snapshot);
            exposed.insert(exposed.end(), child_ids.begin(), child_ids.end());
        }
        return exposed;
    }

    SemanticNodeSnapshot semantic_node;
    semantic_node.id = static_cast<SemanticId>(source.id);
    semantic_node.parent = semantic_parent;
    semantic_node.info = std::move(info);
    semantic_node.bounds = source.bounds;
    if (const auto* virtual_source =
            dynamic_cast<const VirtualSemanticChildrenSource*>(source.component.get())) {
        semantic_node.virtual_children =
            virtual_source->virtual_semantic_children(semantic_node.bounds);
    }

    const auto node_index = snapshot.nodes.size();
    snapshot.nodes.push_back(std::move(semantic_node));

    for (const auto& child : source.children) {
        if (!child) continue;
        auto child_ids = append_semantic_nodes(
            *child,
            snapshot.nodes[node_index].id,
            InheritedSemanticFields{},
            focused_node_id,
            snapshot);
        auto& children = snapshot.nodes[node_index].children;
        children.insert(children.end(), child_ids.begin(), child_ids.end());
    }

    return {snapshot.nodes[node_index].id};
}

} // namespace semantic_tree_detail

/// Build one immutable semantic projection from the retained tree at a UI-thread
/// checkpoint. The snapshot owns values only; no Node/Component pointer is
/// retained after this call returns. Hidden/Collapsed subtrees are omitted and
/// role-None wrappers are flattened while their help description is inherited by
/// the nearest exposed descendants that do not provide their own description.
[[nodiscard]] inline SemanticTreeSnapshot build_semantic_tree_snapshot(
    const Node& root,
    NodeId focused_node_id = kInvalidNodeId) {
    SemanticTreeSnapshot snapshot;
    const auto roots = semantic_tree_detail::append_semantic_nodes(
        root,
        kInvalidSemanticId,
        semantic_tree_detail::InheritedSemanticFields{},
        focused_node_id,
        snapshot);
    if (roots.size() == 1) {
        snapshot.root = roots.front();
    }
    return snapshot;
}

} // namespace ui::detail
