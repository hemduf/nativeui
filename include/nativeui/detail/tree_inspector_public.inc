    [[nodiscard]] debug::InspectorSnapshot inspector_snapshot() {
        flush_dynamic_mutations();

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
            out.clip_bounds = inherited_clip;
            out.layout_dirty = node.layout_dirty;
            out.paint_dirty = paint_intersects_dirty(node.bounds);
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

    /// Paint normal retained content and return the exact diagnostic snapshot
    /// for that frame before dirty regions are cleared. This is a dedicated
    /// inspector path rather than an application overlay, so it cannot affect
    /// retained z-order, hit testing, focus, or T061 overlay ownership.
    [[nodiscard]] debug::InspectorSnapshot paint_with_inspector_snapshot(
        SkCanvas& canvas,
        PlatformServices& platform) {
        flush_dynamic_mutations();
        if (active_) {
            sync_availability_structure(platform);
            sync_focus_structure(platform);
        }
        ensure_layout();

        auto snapshot = inspector_snapshot();
        Painter painter{canvas};
        paint_node(*root_, painter, platform, viewport_rect());
        paint_dirty_.clear();
        return snapshot;
    }
