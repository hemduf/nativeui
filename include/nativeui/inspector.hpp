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

    [[nodiscard]] const InspectorNode* find(NodeId id) const noexcept {
        for (const auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
};

#if defined(NATIVEUI_ENABLE_INSPECTOR)
[[nodiscard]] bool inspector_enabled(const UI& ui) noexcept;
void set_inspector_enabled(UI& ui, bool enabled);
[[nodiscard]] NodeId inspector_selected_node(const UI& ui) noexcept;
void set_inspector_selected_node(UI& ui, NodeId id);
[[nodiscard]] InspectorSnapshot inspector_snapshot(UI& ui);
#endif

} // namespace debug
} // namespace ui
