    void set_overlay_service(detail::OverlayService* service) noexcept {
        overlay_service_ = service;
    }

    [[nodiscard]] std::optional<SemanticInfo> component_semantics(NodeId id) const noexcept {
        try {
            SemanticOperationScope guard{*const_cast<Tree*>(this)};
            if (!root_) return std::nullopt;
            auto* node = find_node_mutable(*root_, id);
            if (!node) return std::nullopt;
            return const_cast<Tree*>(this)->decorated_component_semantics(*node);
        } catch (...) {
            // Keep the historic diagnostic API noexcept. A failed projection
            // is unavailable for this read and can recover on a later read.
            return std::nullopt;
        }
    }

    /// Per-UI global dismissal for transient retained presentations (for
    /// example a pending/visible Tooltip). Runs the mounted-tree policy hook
    /// once; there is deliberately no process-wide or per-application registry.
    void dismiss_transient_presentations() {
        if (!root_) return;
        dismiss_transient_node(*root_);
    }

    static void dismiss_transient_node(Node& node) {
        if (auto* presentation =
                dynamic_cast<detail::TransientPresentation*>(node.component.get())) {
            presentation->dismiss_transient_presentation();
        }
        for (auto& child : node.children) dismiss_transient_node(*child);
    }
