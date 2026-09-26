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

    /// T068 live-state resolver used by the owning native view after T065 has
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

    /// Re-resolve and dispatch one T068 action against the committed retained
    /// tree. Eligibility is rechecked by dispatch_live_semantic_action(); no
    /// Node/Component pointer is retained after the handler begins and lifecycle
    /// transitions reject rather than attempting a synchronous fallback.
    [[nodiscard]] bool dispatch_semantic_action(
        const detail::SemanticIdentity& identity,
        const detail::SemanticActionRequest& request) {
        if (!root_ || lifecycle_transition_active()) {
            return false;
        }
        return detail::dispatch_live_semantic_action(
            *root_, identity, request, semantic_action_focused_node_id());
    }
