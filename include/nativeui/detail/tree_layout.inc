    [[nodiscard]] Rect viewport_rect() const noexcept {
        return Rect{0.0f, 0.0f, std::max(0.0f, viewport_.w), std::max(0.0f, viewport_.h)};
    }

    [[nodiscard]] std::function<void()> paint_invalidator(Node& node) {
        return [this, &node] { invalidate_node_paint(node); };
    }

    [[nodiscard]] std::function<void()> layout_invalidator(Node& node) {
        return [this, &node] { invalidate_layout_from(node); };
    }

    [[nodiscard]] std::function<void()> focus_invalidator(Node& node) {
        return [this, &node] {
            focus_structure_dirty_ = true;
            invalidate_node_paint(node);
        };
    }

    void invalidate_node_paint(Node& node) {
        invalidate_paint(node.bounds.empty() ? viewport_rect() : node.bounds);
    }

    void invalidate_paint(Rect rect) {
        if (const auto exposed = paint_dirty_.add(rect, viewport_rect())) {
            if (invalidation_callback_) invalidation_callback_(*exposed);
        }
    }

    void invalidate_layout_from(Node& node) {
        for (Node* current = &node; current; current = current->parent) {
            current->layout_dirty = true;
        }
        layout_dirty_ = true;
        // Bounds can move on re-layout, so the safe invalidation is the full
        // viewport until retained old/new bounds tracking is introduced.
        invalidate_paint(viewport_rect());
    }

    void ensure_layout() {
        if (!layout_dirty_) return;
        layout_node(*root_, viewport_rect());
        layout_dirty_ = false;
    }

    static std::vector<ChildMetrics> measure_children(
        const Node& node, const Constraints& constraints) {
        std::vector<ChildMetrics> child_metrics;
        child_metrics.reserve(node.children.size());
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const auto child_constraints =
                node.component->child_constraints(constraints, i, node.children.size());
            child_metrics.push_back(measure_node(*node.children[i], child_constraints));
        }
        return child_metrics;
    }

    static ChildMetrics measure_node(const Node& node, const Constraints& constraints) {
        const auto child_metrics = measure_children(node, constraints);
        auto metrics = node.component->measure_constrained(constraints, child_metrics);
        metrics.flex = node.component->flex_factors();
        return metrics;
    }

    static Rect sanitize_placement(Rect placement, Rect fallback) noexcept {
        if (!std::isfinite(placement.x)) placement.x = fallback.x;
        if (!std::isfinite(placement.y)) placement.y = fallback.y;
        if (!std::isfinite(placement.w) || placement.w < 0.0f) placement.w = 0.0f;
        if (!std::isfinite(placement.h) || placement.h < 0.0f) placement.h = 0.0f;
        return placement;
    }

    static void layout_node(Node& node, Rect bounds) {
        bounds.w = std::isfinite(bounds.w) ? std::max(0.0f, bounds.w) : 0.0f;
        bounds.h = std::isfinite(bounds.h) ? std::max(0.0f, bounds.h) : 0.0f;
        node.bounds = bounds;

        const auto constraints = Constraints::tight({bounds.w, bounds.h});
        const auto child_metrics = measure_children(node, constraints);

        std::vector<ChildPlacement> placements(node.children.size());
        node.component->layout_children(bounds, child_metrics, placements);

        for (std::size_t i = 0; i < node.children.size(); ++i) {
            layout_node(*node.children[i], sanitize_placement(placements[i].bounds, bounds));
        }
        node.layout_dirty = false;
    }

    void mount_node(Node& node) {
        MountContext context{
            node.id, paint_invalidator(node), layout_invalidator(node), focus_invalidator(node)};
        node.component->mount(context);
        if (node.component->focusable()) focusables_.push_back(&node);
        for (auto& child : node.children) mount_node(*child);
    }

    void activate_node(Node& node) {
        LifecycleContext context{
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
        node.component->activate(context);
        for (auto& child : node.children) activate_node(*child);
    }

    void deactivate_node(Node& node) {
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            deactivate_node(**it);
        }
        LifecycleContext context{
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
        node.component->deactivate(context);
    }

    void unmount_node(Node& node) {
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            unmount_node(**it);
        }
        LifecycleContext context{
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
        node.component->unmount(context);
    }

    void paint_node(
        const Node& node,
        Painter& painter,
        PlatformServices& platform,
        Rect inherited_clip) const {
        if (inherited_clip.empty()) return;

        const bool focused =
            focus_active_ && !focusables_.empty() && focusables_[focused_index_] == &node;

        PaintContext context{painter, node.bounds, inherited_clip, focused, platform};
        {
            auto paint_scope = painter.scoped_state();
            node.component->paint(context);
        }

        if (node.component->clips_children()) {
            const Rect child_clip = intersect(inherited_clip, node.bounds);
            if (child_clip.empty()) return;
            painter.push_clip(node.bounds);
            for (const auto& child : node.children) {
                paint_node(*child, painter, platform, child_clip);
            }
            painter.pop_clip();
            return;
        }

        for (const auto& child : node.children) {
            paint_node(*child, painter, platform, inherited_clip);
        }
    }

    static Node* hit_test_focusable(Node& node, Point p, Rect inherited_clip) {
        if (inherited_clip.empty() || !inherited_clip.contains(p)) return nullptr;
        if (node.component->is_focus_scope() && !node.component->focus_scope_active()) {
            return nullptr;
        }

        Rect child_clip = inherited_clip;
        if (node.component->clips_children()) {
            child_clip = intersect(inherited_clip, node.bounds);
        }

        if (!child_clip.empty()) {
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                if (auto* result = hit_test_focusable(**it, p, child_clip)) return result;
            }
        }
        if (node.component->focusable() && node.bounds.contains(p)) return &node;
        return nullptr;
    }

    // Deepest visual node under a point, independent of keyboard focusability.
    // Used by drag-and-drop so transparent wrappers/custom drop targets can
    // receive offers via normal bubbling without becoming keyboard-focusable.
    static Node* hit_test_any(Node& node, Point p, Rect inherited_clip) {
        if (inherited_clip.empty() || !inherited_clip.contains(p)) return nullptr;

        Rect child_clip = inherited_clip;
        if (node.component->clips_children()) child_clip = intersect(inherited_clip, node.bounds);
        if (!child_clip.empty()) {
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                if (auto* result = hit_test_any(**it, p, child_clip)) return result;
            }
        }
        return node.bounds.contains(p) ? &node : nullptr;
    }

    [[nodiscard]] static bool is_descendant_or_self(const Node* node, const Node* ancestor) noexcept {
