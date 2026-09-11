    [[nodiscard]] debug::InspectorSnapshot inspector_snapshot() {
        flush_dynamic_mutations();
        ensure_layout();

        debug::InspectorSnapshot snapshot;
        snapshot.dirty_regions = paint_dirty_.rects();
        if (!root_) return snapshot;

        const Node* focused = nullptr;
        if (focus_active_ && focused_index_ < focusables_.size()) {
            focused = focusables_[focused_index_];
        }
        const NodeId capture_owner = pointer_capture_.node ? pointer_capture_.owner : kInvalidNodeId;

        const auto paint_intersects_dirty = [this](Rect bounds) noexcept {
            for (const auto& dirty : paint_dirty_.rects()) {
                if (!intersect(bounds, dirty).empty()) return true;
            }
            return false;
        };

        auto append = [&](auto&& self,
                          const Node& node,
                          NodeId parent_id,
                          std::size_t depth,
                          std::size_t child_order,
                          Rect inherited_clip) -> void {
            debug::InspectorNode out{};
            out.id = node.id;
            out.parent_id = parent_id;
            out.debug_name = "Component";
            out.bounds = node.bounds;
            out.clip_bounds = intersect(inherited_clip, node.bounds);
            out.layout_dirty = node.layout_dirty;
            out.paint_dirty = paint_intersects_dirty(out.clip_bounds);
            out.focusable = node.component->focusable();
            out.focused = focused == &node;
            out.pointer_capture_owner = capture_owner == node.id;
            out.availability = node.component->effective_availability();
            out.child_order = child_order;
            out.depth = depth;
            snapshot.nodes.push_back(std::move(out));

            Rect child_clip = inherited_clip;
            if (node.component->clips_children()) {
                child_clip = intersect(inherited_clip, node.bounds);
            }

            for (std::size_t index = 0; index < node.children.size(); ++index) {
                self(self, *node.children[index], node.id, depth + 1, index, child_clip);
            }
        };

        append(append, *root_, kInvalidNodeId, 0, 0, viewport_rect());
        return snapshot;
    }
