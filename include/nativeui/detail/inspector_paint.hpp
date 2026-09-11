#pragma once

#include <nativeui/inspector.hpp>
#include <nativeui/paint.hpp>

#include <string>

namespace ui::detail {

inline void paint_inspector_overlay(SkCanvas& canvas,
                                    const debug::InspectorSnapshot& snapshot,
                                    NodeId selected) {
    Painter painter{canvas};
    auto state = painter.scoped_state();

    constexpr Color kNodeColor{0.15f, 0.72f, 1.0f, 0.88f};
    constexpr Color kClipColor{0.94f, 0.73f, 0.18f, 0.72f};
    constexpr Color kDirtyColor{1.0f, 0.25f, 0.28f, 0.82f};
    constexpr Color kSelectedColor{0.50f, 1.0f, 0.38f, 1.0f};
    constexpr Color kLabelColor{1.0f, 1.0f, 1.0f, 0.96f};

    for (const auto& dirty : snapshot.dirty_regions) {
        if (!dirty.empty()) painter.stroke_rounded_rect(dirty, 0.0f, 1.0f, kDirtyColor);
    }

    for (const auto& node : snapshot.nodes) {
        if (node.bounds.empty()) continue;
        const bool is_selected = node.id == selected;
        painter.stroke_rounded_rect(
            node.bounds, 0.0f, is_selected ? 2.0f : 1.0f,
            is_selected ? kSelectedColor : kNodeColor);

        if (!node.clip_bounds.empty() &&
            !(node.clip_bounds.x == node.bounds.x &&
              node.clip_bounds.y == node.bounds.y &&
              node.clip_bounds.w == node.bounds.w &&
              node.clip_bounds.h == node.bounds.h)) {
            painter.stroke_rounded_rect(node.clip_bounds, 0.0f, 1.0f, kClipColor);
        }

        std::string label = "#" + std::to_string(node.id);
        if (!node.debug_name.empty()) {
            label += " ";
            label += node.debug_name;
        }
        painter.text({node.bounds.x + 3.0f, node.bounds.y + 11.0f},
                     label, 9.0f, kLabelColor);
    }
}

} // namespace ui::detail
