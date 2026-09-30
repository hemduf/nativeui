    /// Current focused retained identity used only while resolving one semantic
    /// query/action on the UI thread. The value is copied before component code
    /// is entered; no Node pointer escapes this helper.
    [[nodiscard]] NodeId semantic_action_focused_node_id() const noexcept {
        if (!focus_active_ || focused_index_ >= focusables_.size()) {
            return kInvalidNodeId;
        }
        const auto* focused = focusables_[focused_index_];
        return focused ? focused->id : kInvalidNodeId;
    }

    /// accessibility live-state resolver used by the owning native view after dispatcher has
    /// marshalled a platform request onto the UI thread. It borrows the retained
    /// root for this call only and fails closed during lifecycle transitions.
    [[nodiscard]] std::optional<SemanticInfo> current_semantics_for_action(
        const detail::SemanticIdentity& identity) const {
        if (!root_ || lifecycle_transition_active()) {
            return std::nullopt;
        }
        return detail::current_live_semantics(
            *root_, identity, semantic_action_focused_node_id());
    }

    /// Move retained keyboard focus to one exposed semantic identity.
    ///
    /// Composite widgets expose semantic children (one Tab, one RangeSlider
    /// handle, one ListView row) that intentionally stay outside `focusables_`
    /// so the widget remains one keyboard Tab stop. The request therefore
    /// resolves to the nearest retained focusable ancestor and fails closed when
    /// the tree has no active platform focus domain, the identity is gone, or
    /// the resolved owner is currently unavailable.
    [[nodiscard]] bool request_semantic_focus(SemanticId id) {
        if (!root_ || !active_ || !active_platform_ || lifecycle_transition_active()) {
            return false;
        }
        for (auto* node = find_node_mutable(*root_, id); node; node = node->parent) {
            if (std::find(focusables_.begin(), focusables_.end(), node) ==
                focusables_.end()) {
                continue;
            }
            if (!focus_target_available(node)) return false;
            set_focus(node, *active_platform_);
            // set_focus() applies synchronously, queues behind an in-flight
            // focus transition, or silently rejects the target (trapping scope).
            // Report acceptance only for the first two cases.
            return current_focus_node() == node ||
                   (pending_focus_request_ && pending_focus_target_ == node->id);
        }
        return false;
    }

    /// Re-resolve and dispatch one accessibility action against the committed retained
    /// tree. Eligibility is rechecked by dispatch_live_semantic_action(); no
    /// Node/Component pointer is retained after the handler begins and lifecycle
    /// transitions reject rather than attempting a synchronous fallback. Focus
    /// requests are routed back through this tree's retained focus manager.
    [[nodiscard]] bool dispatch_semantic_action(
        const detail::SemanticIdentity& identity,
        const detail::SemanticActionRequest& request) {
        if (!root_ || lifecycle_transition_active()) {
            return false;
        }
        return detail::dispatch_live_semantic_action(
            *root_,
            identity,
            request,
            semantic_action_focused_node_id(),
            [this](SemanticId id) { return request_semantic_focus(id); });
    }
