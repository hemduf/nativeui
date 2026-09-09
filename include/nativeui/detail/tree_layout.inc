    struct PendingAvailability {
        Node* node{};
        ComponentAvailability value{};
    };

    [[nodiscard]] static ComponentAvailability combine_availability(
        ComponentAvailability parent, ComponentAvailability local) noexcept {
        ComponentAvailability result{};
        if (parent.visibility == VisibilityMode::Collapsed ||
            local.visibility == VisibilityMode::Collapsed) {
            result.visibility = VisibilityMode::Collapsed;
        } else if (parent.visibility == VisibilityMode::Hidden ||
                   local.visibility == VisibilityMode::Hidden) {
            result.visibility = VisibilityMode::Hidden;
        } else {
            result.visibility = VisibilityMode::Visible;
        }
        result.enabled = parent.enabled && local.enabled;
        result.read_only = parent.read_only || local.read_only;
        return result;
    }

    static void initialize_availability(
        Node& node, ComponentAvailability parent = ComponentAvailability{}) noexcept {
        const auto effective = combine_availability(parent, node.component->local_availability());
        node.component->set_effective_availability(effective);
        for (auto& child : node.children) initialize_availability(*child, effective);
    }

    static void collect_availability(
        Node& node,
        ComponentAvailability parent,
        std::vector<PendingAvailability>& pending) {
        const auto effective = combine_availability(parent, node.component->local_availability());
        pending.push_back(PendingAvailability{&node, effective});
        for (auto& child : node.children) collect_availability(*child, effective, pending);
    }

    [[nodiscard]] static bool availability_differs(
        const Node& node, ComponentAvailability parent = ComponentAvailability{}) noexcept {
        const auto effective = combine_availability(parent, node.component->local_availability());
        if (!(effective == node.component->effective_availability())) return true;
        for (const auto& child : node.children) {
            if (availability_differs(*child, effective)) return true;
        }
        return false;
    }

    [[nodiscard]] std::function<void()> availability_invalidator(Node& node) {
        return [this, &node] {
            if (!root_ || !availability_differs(*root_)) return;
            availability_structure_dirty_ = true;
            if (!active_) {
                sync_availability_inactive();
                return;
            }
            if (active_platform_ && dispatch_depth_ == 0 && !syncing_availability_) {
                sync_availability_structure(*active_platform_);
                return;
            }
            // A component callback is currently on the stack. Schedule a frame;
            // dispatch completion will reconcile the transition before returning
            // to the platform event loop.
            invalidate_node_paint(node);
        };
    }

    void apply_pending_availability(
        const std::vector<PendingAvailability>& pending,
        bool& layout_changed,
        bool& focus_changed,
        bool& paint_changed) {
        for (const auto& change : pending) {
            const auto old = change.node->component->effective_availability();
            if (old == change.value) continue;

            const bool old_collapsed = old.visibility == VisibilityMode::Collapsed;
            const bool new_collapsed = change.value.visibility == VisibilityMode::Collapsed;
            layout_changed = layout_changed || old_collapsed != new_collapsed;
            focus_changed = focus_changed ||
                old.visibility != change.value.visibility || old.enabled != change.value.enabled;
            paint_changed = true;
            change.node->component->set_effective_availability(change.value);
        }
    }

    void sync_availability_inactive() {
        if (!availability_structure_dirty_ || !root_) return;

        std::vector<PendingAvailability> pending;
        pending.reserve(focusables_.size() + 8);
        collect_availability(*root_, ComponentAvailability{}, pending);
        availability_structure_dirty_ = false;

        bool layout_changed = false;
        bool focus_changed = false;
        bool paint_changed = false;
        apply_pending_availability(pending, layout_changed, focus_changed, paint_changed);

        if (focus_changed) focus_structure_dirty_ = true;
        if (layout_changed) {
            invalidate_layout_from(*root_);
        } else if (paint_changed) {
            invalidate_paint(viewport_rect());
        }
    }

    [[nodiscard]] static ComponentAvailability pending_availability_for(
        const std::vector<PendingAvailability>& pending, const Node* node) noexcept {
        if (!node) return {};
        for (const auto& change : pending) {
            if (change.node == node) return change.value;
        }
        return node->component->effective_availability();
    }

    void sync_availability_structure(PlatformServices& platform) {
        if (!availability_structure_dirty_ || syncing_availability_ || !root_) return;

        syncing_availability_ = true;
        Node* rehome_scope = nullptr;
        bool focus_needs_rehome = false;
        constexpr int kMaxReconcilePasses = 16;

        for (int pass = 0; pass < kMaxReconcilePasses && availability_structure_dirty_; ++pass) {
            availability_structure_dirty_ = false;

            std::vector<PendingAvailability> pending;
            pending.reserve(focusables_.size() + 8);
            collect_availability(*root_, ComponentAvailability{}, pending);

            bool any_change = false;
            for (const auto& change : pending) {
                if (!(change.value == change.node->component->effective_availability())) {
                    any_change = true;
                    break;
                }
            }
            if (!any_change) {
                // A teardown callback can reverse the state that triggered
                // focus loss before the pending availability snapshot is
                // applied. In that case there is no availability delta on the
                // next pass, but focus still needs to be restored explicitly.
                if (focus_needs_rehome && active_ && !focus_active_) {
                    Node* fallback = nullptr;
                    if (rehome_scope &&
                        rehome_scope->component->effective_availability().interactive()) {
                        fallback = first_available_focusable(rehome_scope);
                    } else {
                        fallback = first_available_focusable();
                    }
                    if (fallback) set_focus(fallback, platform);
                    focus_needs_rehome = false;
                    rehome_scope = nullptr;
                }
                continue;
            }

            if (auto* capture = pointer_capture_owner()) {
                if (!pending_availability_for(pending, capture).interactive()) {
                    (void)cancel_pointer_capture(platform);
                }
            }

            if (auto* focused = current_focus_node()) {
                if (!pending_availability_for(pending, focused).interactive()) {
                    rehome_scope = trapping_scope_for(focused);
                    focus_needs_rehome = true;
                    clear_focus(platform);
                }
            }

            // PointerCancel/focus callbacks are allowed to mutate State. If they
            // changed availability, discard this snapshot and recompute before
            // applying suppression so callbacks always observe the old valid
            // interaction context.
            if (availability_structure_dirty_) continue;

            bool layout_changed = false;
            bool focus_changed = false;
            bool paint_changed = false;
            apply_pending_availability(pending, layout_changed, focus_changed, paint_changed);

            if (focus_changed) focus_structure_dirty_ = true;
            if (layout_changed) {
                invalidate_layout_from(*root_);
            } else if (paint_changed) {
                invalidate_paint(viewport_rect());
            }

            if (focus_changed && active_) sync_focus_structure(platform);
            if (focus_needs_rehome && active_ && !focus_active_) {
                Node* fallback = nullptr;
                if (rehome_scope && rehome_scope->component->effective_availability().interactive()) {
                    fallback = first_available_focusable(rehome_scope);
                } else {
                    fallback = first_available_focusable();
                }
                if (fallback) set_focus(fallback, platform);
                focus_needs_rehome = false;
                rehome_scope = nullptr;
            }
        }

        syncing_availability_ = false;
        if (availability_structure_dirty_) {
            // A hostile callback loop must not recurse without bound. Preserve
            // the dirty bit and schedule another deterministic reconciliation.
            invalidate_paint(viewport_rect());
        }
    }

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
        if (node.component->effective_visibility() == VisibilityMode::Collapsed) {
            return ChildMetrics{Size{}, Size{}, FlexFactors{}};
        }
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

        if (node.component->effective_visibility() == VisibilityMode::Collapsed) {
            node.bounds = Rect{bounds.x, bounds.y, 0.0f, 0.0f};
            for (auto& child : node.children) layout_node(*child, node.bounds);
            node.layout_dirty = false;
            return;
        }

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
            node.id,
            paint_invalidator(node),
            layout_invalidator(node),
            focus_invalidator(node),
            availability_invalidator(node)};
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
        if (inherited_clip.empty() ||
            node.component->effective_visibility() != VisibilityMode::Visible) {
            return;
        }

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
        if (inherited_clip.empty() || !inherited_clip.contains(p) ||
            !node.component->effective_availability().interactive()) {
            return nullptr;
        }
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
        if (inherited_clip.empty() || !inherited_clip.contains(p) ||
            !node.component->effective_availability().interactive()) {
            return nullptr;
        }

        Rect child_clip = inherited_clip;
        if (node.component->clips_children()) child_clip = intersect(inherited_clip, node.bounds);
        if (!child_clip.empty()) {
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                if (auto* result = hit_test_any(**it, p, child_clip)) return result;
            }
        }
        return node.bounds.contains(p) ? &node : nullptr;
    }

    [[nodiscard]] static const Node* find_node(const Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (const auto& child : node.children) {
            if (const auto* result = find_node(*child, id)) return result;
        }
        return nullptr;
    }

    [[nodiscard]] static bool is_descendant_or_self(const Node* node, const Node* ancestor) noexcept {