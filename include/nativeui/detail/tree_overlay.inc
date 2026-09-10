    [[nodiscard]] ChildMetrics measure_overlay_content(
        const Constraints& constraints) const {
        const_cast<Tree*>(this)->flush_dynamic_mutations();
        if (!root_ || root_->children.empty()) return {};
        return measure_node(*root_->children.front(), constraints);
    }

    void layout_overlay_viewport(Size viewport) {
        flush_dynamic_mutations();
        if (!root_ || root_->children.empty()) {
            layout(viewport);
            return;
        }

        const bool size_changed = viewport.w != viewport_.w || viewport.h != viewport_.h;
        if (!size_changed && !layout_dirty_) return;

        viewport_ = viewport;
        paint_dirty_.clear();
        const Rect bounds = viewport_rect();
        root_->bounds = bounds;

        // OverlayHost always keeps the application root at child 0 and never
        // consumes that child's measured size when assigning its full viewport
        // bounds. Avoid recursively measuring the complete application subtree
        // once here only to measure it again inside layout_node(). This keeps
        // ordinary UIs on the pre-T061 layout cost while overlays remain
        // independent siblings above the application content.
        if (root_->children.size() == 1) {
            layout_node(*root_->children.front(), bounds);
            root_->layout_dirty = false;
            layout_dirty_ = false;
            invalidate_paint(bounds);
            return;
        }

        const auto constraints = Constraints::tight({bounds.w, bounds.h});
        std::vector<ChildMetrics> child_metrics(root_->children.size());
        for (std::size_t i = 1; i < root_->children.size(); ++i) {
            const auto child_constraints =
                root_->component->child_constraints(constraints, i, root_->children.size());
            child_metrics[i] = measure_node(*root_->children[i], child_constraints);
        }

        std::vector<ChildPlacement> placements(root_->children.size());
        root_->component->layout_children(bounds, child_metrics, placements);
        for (std::size_t i = 0; i < root_->children.size(); ++i) {
            layout_node(*root_->children[i], sanitize_placement(placements[i].bounds, bounds));
        }
        root_->layout_dirty = false;
        layout_dirty_ = false;
        invalidate_paint(bounds);
    }

    [[nodiscard]] std::optional<Rect> overlay_anchor_bounds(NodeId id) const noexcept {
        if (!root_ || id == kInvalidNodeId) return std::nullopt;
        const auto* node = find_node(*root_, id);
        if (!node) return std::nullopt;
        return node->bounds;
    }
