        for (auto* current = node; current; current = current->parent) {
            if (current == ancestor) return true;
        }
        return false;
    }

    [[nodiscard]] bool focus_target_available(const Node* node) const noexcept {
        if (!node || !node->component->effective_availability().interactive()) return false;
        for (auto* current = node; current; current = current->parent) {
            if (!current->component->effective_availability().interactive()) return false;
            if (current->component->is_focus_scope() &&
                !current->component->focus_scope_active()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] Node* current_focus_node() const noexcept {
        return focus_active_ && focused_index_ < focusables_.size()
            ? focusables_[focused_index_]
            : nullptr;
    }

    [[nodiscard]] Node* trapping_scope_for(const Node* node) const noexcept {
        for (auto* current = node; current; current = current->parent) {
            if (current->component->effective_availability().interactive() &&
                current->component->is_focus_scope() &&
                current->component->focus_scope_active() &&
                current->component->focus_scope_traps()) {
                return const_cast<Node*>(current);
            }
        }
        return nullptr;
    }

    [[nodiscard]] Node* first_available_focusable(const Node* within = nullptr) const noexcept {
        for (auto* candidate : focusables_) {
            if (!focus_target_available(candidate)) continue;
            if (within && !is_descendant_or_self(candidate, within)) continue;
            return candidate;
        }
        return nullptr;
    }

    [[nodiscard]] Node* default_focus_for_scope(Node& scope) const noexcept {
        const auto requested = scope.component->focus_scope_default_index();
        std::size_t index = 0;
        Node* first = nullptr;
        for (auto* candidate : focusables_) {
            if (!focus_target_available(candidate) ||
                !is_descendant_or_self(candidate, &scope)) {
                continue;
            }
            if (!first) first = candidate;
            if (index++ == requested) return candidate;
        }
        return first;
    }

    void clear_focus(PlatformServices& platform) {
        if (!focus_active_ || focused_index_ >= focusables_.size()) {
            focus_active_ = false;
            if (!syncing_availability_) availability_cleared_focus_ = nullptr;
            return;
        }
        auto* old = focusables_[focused_index_];
        if (syncing_availability_) {
            auto* trap = trapping_scope_for(old);
            availability_cleared_focus_ =
                trap && trap->focus_restore != kInvalidNodeId ? old : nullptr;
        } else {
            availability_cleared_focus_ = nullptr;
        }
        FocusContext context{
            old->bounds, platform, paint_invalidator(*old), layout_invalidator(*old)};
        old->component->focus_changed(false, context);
        invalidate_node_paint(*old);
        focus_active_ = false;
    }

    void move_focus(int direction, PlatformServices& platform) {
        if (focusables_.empty()) return;
        auto* current = current_focus_node();
        auto* trap = trapping_scope_for(current);
        const auto count = focusables_.size();
        std::size_t start = current ? focused_index_ : 0;
        for (std::size_t step = 1; step <= count; ++step) {
            const auto index = direction > 0
                ? (start + step) % count
                : (start + count - (step % count)) % count;
            auto* candidate = focusables_[index];
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;
            set_focus(candidate, platform);
            return;
        }
    }

    [[nodiscard]] static bool scope_effectively_active(const Node& scope) noexcept {
        if (!scope.component->effective_availability().interactive() ||
            !scope.component->is_focus_scope() || !scope.component->focus_scope_active()) {
            return false;
        }
        for (auto* parent = scope.parent; parent; parent = parent->parent) {
            if (!parent->component->effective_availability().interactive()) return false;
            if (parent->component->is_focus_scope() &&
                !parent->component->focus_scope_active()) {
                return false;
            }
        }
        return true;
    }

    static void reset_focus_scope_cache(Node& node) noexcept {
        node.focus_scope_active_cached = false;
        node.focus_restore = kInvalidNodeId;
        for (auto& child : node.children) reset_focus_scope_cache(*child);
    }

    void reconcile_scope_deactivations(Node& node, PlatformServices& platform) {
        for (auto& child : node.children) reconcile_scope_deactivations(*child, platform);
        if (!node.component->is_focus_scope()) return;
        const bool now = scope_effectively_active(node);
        if (node.focus_scope_active_cached && !now) {
            auto* focused = current_focus_node();
            auto* transition_focus = availability_cleared_focus_ &&
                    is_descendant_or_self(availability_cleared_focus_, &node)
                ? availability_cleared_focus_
                : focused;
            const auto restore_id = node.focus_restore;
            node.focus_scope_active_cached = false;
            node.focus_restore = kInvalidNodeId;
            if (transition_focus && is_descendant_or_self(transition_focus, &node)) {
                Node* restore = restore_id == kInvalidNodeId
                    ? nullptr
                    : find_node_mutable(*root_, restore_id);
                if (focus_target_available(restore)) {
                    set_focus(restore, platform);
                } else if (auto* fallback = first_available_focusable()) {
                    set_focus(fallback, platform);
                } else {
                    clear_focus(platform);
                }
            }
        }
    }

    void reconcile_scope_activations(Node& node, PlatformServices& platform) {
        if (node.component->is_focus_scope()) {
            const bool now = scope_effectively_active(node);
            if (!node.focus_scope_active_cached && now) {
                node.focus_scope_active_cached = true;
                if (auto* restore = current_focus_node()) {
                    node.focus_restore = restore->id;
                } else {
                    node.focus_restore = kInvalidNodeId;
                }
                if (auto* target = default_focus_for_scope(node)) {
                    set_focus(target, platform);
                }
            }
        }
        for (auto& child : node.children) reconcile_scope_activations(*child, platform);
    }

    void sync_focus_structure(PlatformServices& platform) {
        if (!focus_structure_dirty_) return;
        reconcile_scope_deactivations(*root_, platform);
        reconcile_scope_activations(*root_, platform);
        if (focus_active_ && !focus_target_available(current_focus_node())) {
            if (auto* fallback = first_available_focusable()) set_focus(fallback, platform);
            else clear_focus(platform);
        }
        availability_cleared_focus_ = nullptr;
        focus_structure_dirty_ = false;
    }

    void set_focus(Node* node, PlatformServices& platform) {
        if (!node || !focus_target_available(node)) return;
        const auto it = std::find(focusables_.begin(), focusables_.end(), node);
        if (it == focusables_.end()) return;

        const auto next_index = static_cast<std::size_t>(std::distance(focusables_.begin(), it));
        if (focus_active_ && next_index == focused_index_) return;

        if (focus_active_ && !focusables_.empty()) {
            auto* old = focusables_[focused_index_];
            FocusContext old_context{
                old->bounds, platform, paint_invalidator(*old), layout_invalidator(*old)};
            old->component->focus_changed(false, old_context);
            invalidate_node_paint(*old);
        }

        focused_index_ = next_index;
        focus_active_ = true;

        FocusContext new_context{
            node->bounds, platform, paint_invalidator(*node), layout_invalidator(*node)};
        node->component->focus_changed(true, new_context);
        invalidate_node_paint(*node);

        for (auto* ancestor = node->parent; ancestor; ancestor = ancestor->parent) {
            ancestor->component->descendant_focus_changed(Rect{
                node->bounds.x - ancestor->bounds.x,
                node->bounds.y - ancestor->bounds.y,
                node->bounds.w,
                node->bounds.h});
        }

        if (!syncing_availability_ || !focus_structure_dirty_) {
            availability_cleared_focus_ = nullptr;
        }
    }

    [[nodiscard]] EventResult route_from(