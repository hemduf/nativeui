#pragma once

#include <nativeui/component_base.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace ui {

class UI;

namespace debug {

/// Immutable value snapshot for one retained node. This intentionally contains
/// no Node*, Component*, callbacks, references, or other runtime ownership.
struct InspectorNode {
    NodeId id{kInvalidNodeId};
    NodeId parent_id{kInvalidNodeId};
    std::string debug_name{"Component"};
    Rect bounds{};
    Rect clip_bounds{};
    bool layout_dirty{};
    bool paint_dirty{};
    bool focusable{};
    bool focused{};
    bool pointer_capture_owner{};
    ComponentAvailability availability{};
    std::size_t child_order{};
    std::size_t depth{};
};

/// Frame/update-local diagnostic data copied from a UI tree. Consumers may keep
/// or copy this object after the retained tree changes because it owns all data.
struct InspectorSnapshot {
    std::vector<InspectorNode> nodes;
    std::vector<Rect> dirty_regions;

    /// Find one node in this owned snapshot. The returned pointer is borrowed
    /// from this InspectorSnapshot and is invalidated by snapshot mutation or
    /// destruction.
    [[nodiscard]] const InspectorNode* find(NodeId id) const noexcept {
        for (const auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
};

#if defined(NATIVEUI_ENABLE_INSPECTOR)
/// Query whether the passive debug overlay is enabled for this UI instance.
[[nodiscard]] bool inspector_enabled(const UI& ui) noexcept;

/// Enable/disable the per-UI passive inspector overlay and invalidate the UI.
void set_inspector_enabled(UI& ui, bool enabled);

/// Read the currently selected retained NodeId for inspector highlighting.
[[nodiscard]] NodeId inspector_selected_node(const UI& ui) noexcept;

/// Select a retained NodeId for inspector highlighting. Stale/missing IDs remain
/// safe diagnostic input and do not create retained ownership.
void set_inspector_selected_node(UI& ui, NodeId id);

/// Copy an owned point-in-time retained-tree diagnostic snapshot.
[[nodiscard]] InspectorSnapshot inspector_snapshot(UI& ui);
#endif

} // namespace debug
} // namespace ui
