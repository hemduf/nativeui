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
            if (!root_) return;
            // Any local availability mutation that occurs while a reconciliation
            // snapshot is active invalidates that snapshot, even when the new
            // local state happens to match the still-unapplied effective state.
            // This is what makes focus/capture teardown callbacks safely able to
            // reverse the transition that triggered them.
            if (syncing_availability_) {
                availability_structure_dirty_ = true;
                return;
            }
            if (!availability_differs(*root_)) return;
            availability_structure_dirty_ = true;
            if (!active_) {
                sync_availability_inactive();
                return;
            }
            if (active_platform_ && dispatch_depth_ == 0) {
                sync_availability_structure(*active_platform_);
                return;
            }
            // A component callback is currently on the stack. Schedule a frame;
            // dispatch completion will reconcile the transition before returning
            // to the platform event loop.
            invalidate_node_paint(node);
        };
    }

    [[nodiscard]] static bool measured_availability_change_affects_layout(
        Node& node,
        const ComponentAvailability& before,
        const ComponentAvailability& after) {
        if (before.enabled == after.enabled && before.read_only == after.read_only) return false;
        if (dynamic_cast<detail::ThemeBinding*>(node.component.get()) == nullptr) return false;

        // T038 style-aware components may expose availability variants that alter
        // intrinsic measurement without needing a bespoke tree hook. Reuse the
        // already-laid-out child extents as stable inputs so this fallback stays
        // bounded and does not recursively remeasure an entire subtree.
        std::vector<ChildMetrics> children;
        children.reserve(node.children.size());
        for (const auto& child : node.children) {
            const Size current{
                std::isfinite(child->bounds.w) ? std::max(0.0f, child->bounds.w) : 0.0f,
                std::isfinite(child->bounds.h) ? std::max(0.0f, child->bounds.h) : 0.0f};
            children.emplace_back(current, current, child->component->flex_factors());
        }

        const auto before_size = node.component->measure(children);
        node.component->set_effective_availability(after);
        Size after_size{};
        try {
            after_size = node.component->measure(children);
        } catch (...) {
            node.component->set_effective_availability(before);
            throw;
        }
        node.component->set_effective_availability(before);
        return before_size.w != after_size.w || before_size.h != after_size.h;
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
            bool style_layout_changed =
                change.node->component->availability_change_affects_layout(old, change.value);
            if (!style_layout_changed) {
                style_layout_changed = measured_availability_change_affects_layout(
                    *change.node, old, change.value);
            }
            layout_changed = layout_changed || old_collapsed != new_collapsed ||
                style_layout_changed;
            focus_changed = focus_changed ||
                old.visibility != change.value.visibility || old.enabled != change.value.enabled;

            bool style_paint_changed = true;
            if (const auto* binding =
                    dynamic_cast<const detail::ThemeBinding*>(change.node->component.get())) {
                style_paint_changed =
                    binding->availability_change_affects_paint(old, change.value);
            }
            paint_changed = paint_changed ||
                old.visibility != change.value.visibility || style_paint_changed;
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
            publish_all_visual_bounds_then_invalidate();
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

            while (const auto pointer = find_pointer_capture_if(
                       [&](Node* capture) {
                           return !pending_availability_for(pending, capture).interactive();
                       })) {
                InputEvent cancel{};
                cancel.pointer.id = *pointer;
                (void)cancel_pointer_capture(platform, cancel);
                if (availability_structure_dirty_) break;
            }

            if (availability_structure_dirty_) continue;

            // A hover route whose target becomes Hidden/Collapsed/Disabled is no
            // longer an eligible interaction. Retained hover presentation must
            // observe PointerLeave exactly as if the pointer had moved away;
            // otherwise a delayed policy such as Tooltip could still fire while
            // its anchor is unavailable.
            if (pointer_hover_target_ &&
                !pending_availability_for(pending, pointer_hover_target_).interactive()) {
                InputEvent leave{};
                leave.type = InputType::PointerLeave;
                (void)clear_pointer_hover(leave, platform);
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
                publish_all_visual_bounds_then_invalidate();
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

    [[nodiscard]] static float canonical_visual_outset_side(float value) noexcept {
        if (!std::isfinite(value) || value <= 0.0f) return 0.0f;
        return value;
    }

    [[nodiscard]] Rect current_visual_bounds(const Node& node) const noexcept {
        if (node.component->effective_visibility() != VisibilityMode::Visible) return {};

        const Rect layout = node.bounds;
        if (!std::isfinite(layout.x) || !std::isfinite(layout.y) ||
            !std::isfinite(layout.w) || !std::isfinite(layout.h) ||
            layout.w < 0.0f || layout.h < 0.0f) {
            return {};
        }

        const auto raw = node.component->visual_outset();
        const double left_outset = canonical_visual_outset_side(raw.left);
        const double top_outset = canonical_visual_outset_side(raw.top);
        const double right_outset = canonical_visual_outset_side(raw.right);
        const double bottom_outset = canonical_visual_outset_side(raw.bottom);

        const Rect viewport = viewport_rect();
        const double viewport_left = static_cast<double>(viewport.x);
        const double viewport_top = static_cast<double>(viewport.y);
        const double viewport_right = viewport_left + static_cast<double>(viewport.w);
        const double viewport_bottom = viewport_top + static_cast<double>(viewport.h);

        const double raw_left = static_cast<double>(layout.x) - left_outset;
        const double raw_top = static_cast<double>(layout.y) - top_outset;
        const double raw_right =
            static_cast<double>(layout.x) + static_cast<double>(layout.w) + right_outset;
        const double raw_bottom =
            static_cast<double>(layout.y) + static_cast<double>(layout.h) + bottom_outset;

        const double left = std::max(viewport_left, raw_left);
        const double top = std::max(viewport_top, raw_top);
        const double right = std::min(viewport_right, raw_right);
        const double bottom = std::min(viewport_bottom, raw_bottom);
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(right) || !std::isfinite(bottom) ||
            !(left < right) || !(top < bottom)) {
            return {};
        }

        return {static_cast<float>(left),
                static_cast<float>(top),
                static_cast<float>(right - left),
                static_cast<float>(bottom - top)};
    }

    void publish_visual_bounds(Node& node) noexcept {
        node.published_visual_bounds = current_visual_bounds(node);
        node.visual_bounds_published = true;
        for (auto& child : node.children) publish_visual_bounds(*child);
    }

    [[nodiscard]] std::optional<Rect> retain_dirty(Rect rect) noexcept {
        return paint_dirty_.add(rect, viewport_rect());
    }

    void notify_invalidation(Rect rect) {
        if (invalidation_callback_) invalidation_callback_(rect);
    }

    void invalidate_node_paint(Node& node) {
        if (layout_transaction_active_ || !node.visual_bounds_published) {
            // Candidate geometry is not publication authority. A state change
            // during layout keeps a conservative repaint pending; successful
            // layout commit will publish the new visual bounds, while rollback
            // leaves the old publication authoritative.
            invalidate_paint(viewport_rect());
            return;
        }

        const Rect old_bounds = node.published_visual_bounds;
        const Rect new_bounds = current_visual_bounds(node);
        const Rect dirty = unite(old_bounds, new_bounds);
        const auto exposed = retain_dirty(dirty);

        node.published_visual_bounds = new_bounds;
        node.visual_bounds_published = true;
        if (exposed) notify_invalidation(*exposed);
    }

    void invalidate_paint(Rect rect) {
        if (const auto exposed = retain_dirty(rect)) {
            notify_invalidation(*exposed);
        }
    }

    void publish_all_visual_bounds_then_invalidate() {
        const auto exposed = retain_dirty(viewport_rect());
        if (!layout_transaction_active_ && root_) publish_visual_bounds(*root_);
        if (exposed) notify_invalidation(*exposed);
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
        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->bind_theme(node.parent ? inherited_theme_for(node) : theme_);
            binding->set_theme_change_invalidator(
                [this, id = node.id](ThemeInvalidation invalidation) {
                    apply_scoped_theme_change(id, invalidation);
                });
        }
        if (dynamic_source(node)) register_dynamic_node(node);
        MountContext context{
            node.id,
            paint_invalidator(node),
            layout_invalidator(node),
            focus_invalidator(node),
            availability_invalidator(node),
            overlay_service_};
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

    static void remember_current_exception(std::exception_ptr& first_error) noexcept {
        if (!first_error) first_error = std::current_exception();
    }

    void deactivate_node(Node& node) {
        std::exception_ptr first_error;
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            try {
                deactivate_node(**it);
            } catch (...) {
                remember_current_exception(first_error);
            }
        }
        LifecycleContext context{
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
        try {
            node.component->deactivate(context);
        } catch (...) {
            remember_current_exception(first_error);
        }
        if (first_error) std::rethrow_exception(first_error);
    }

    void unmount_node(Node& node) {
        std::exception_ptr first_error;
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            try {
                unmount_node(**it);
            } catch (...) {
                remember_current_exception(first_error);
            }
        }
        LifecycleContext context{
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
        try {
            node.component->unmount(context);
        } catch (...) {
            remember_current_exception(first_error);
        }
        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            try {
                binding->set_theme_change_invalidator({});
            } catch (...) {
                remember_current_exception(first_error);
            }
        }
        if (auto* source = dynamic_source(node)) {
            try {
                source->set_structure_invalidator({});
            } catch (...) {
                remember_current_exception(first_error);
            }
            try {
                unregister_dynamic_node(node.id);
            } catch (...) {
                remember_current_exception(first_error);
            }
        }
        if (first_error) std::rethrow_exception(first_error);
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