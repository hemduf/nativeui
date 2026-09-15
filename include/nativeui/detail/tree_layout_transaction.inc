    struct LayoutTransactionCheckpoint {
        Node* node{};
        Rect bounds{};
    };

    struct LayoutPublicationCheckpoint {
        Size viewport{};
        DirtyRegion paint_dirty{};
    };

    void begin_layout_geometry_transaction() {
        // Retained layout callbacks are arbitrary application code. A nested
        // layout attempt would otherwise overwrite the rollback journal of the
        // outer attempt, so fail before mutating geometry and let the outer
        // transaction restore its checkpoint deterministically.
        if (layout_transaction_active_) {
            throw std::logic_error("reentrant retained layout transaction");
        }
        layout_transaction_checkpoints_.clear();
        layout_transaction_active_ = true;
        layout_dirty_ = true;
    }

    void checkpoint_layout_node(Node& node) {
        if (!layout_transaction_active_) {
            throw std::logic_error("layout checkpoint outside transaction");
        }
        // Checkpoint immediately before mutation. Capacity is retained across
        // passes, so steady-state successful layout adds one Rect+pointer write
        // per visited node without a second O(N) traversal or per-pass heap
        // churn. Allocation failure occurs before the current node is mutated
        // and is therefore covered by the same rollback path.
        layout_transaction_checkpoints_.push_back(
            LayoutTransactionCheckpoint{&node, node.bounds});
    }

    void commit_layout_geometry_transaction() noexcept {
        layout_transaction_checkpoints_.clear();
        layout_transaction_active_ = false;
        layout_dirty_ = false;
    }

    void rollback_layout_geometry_transaction() noexcept {
        for (auto it = layout_transaction_checkpoints_.rbegin();
             it != layout_transaction_checkpoints_.rend(); ++it) {
            it->node->bounds = it->bounds;
            // A failed pass must force a complete retry even when this node was
            // clean before the attempt. Tree layout is whole-subtree today, so
            // marking every visited node dirty is sufficient and conservative.
            it->node->layout_dirty = true;
        }
        layout_transaction_checkpoints_.clear();
        layout_transaction_active_ = false;
        layout_dirty_ = true;
    }

    [[nodiscard]] LayoutPublicationCheckpoint begin_layout_publication(Size viewport) {
        LayoutPublicationCheckpoint checkpoint{viewport_, paint_dirty_};
        begin_layout_geometry_transaction();
        viewport_ = viewport;
        // Match the successful-path contract: callbacks during this attempt see
        // a fresh dirty accumulator. On failure the previous bounded dirty
        // region is restored verbatim instead of being lost.
        paint_dirty_.clear();
        return checkpoint;
    }

    void rollback_layout_publication(LayoutPublicationCheckpoint&& checkpoint) noexcept {
        rollback_layout_geometry_transaction();
        viewport_ = checkpoint.viewport;
        paint_dirty_ = std::move(checkpoint.paint_dirty);
    }

    void commit_layout_publication() {
        commit_layout_geometry_transaction();
        paint_dirty_.clear();
        invalidate_paint(viewport_rect());
    }

    void layout_node(Node& node, Rect bounds) {
        const bool owns_transaction = !layout_transaction_active_;
        if (owns_transaction) begin_layout_geometry_transaction();

        try {
            layout_node_transactional(node, bounds);
        } catch (...) {
            if (owns_transaction) rollback_layout_geometry_transaction();
            throw;
        }

        if (owns_transaction) commit_layout_geometry_transaction();
    }

    void ensure_layout() {
        if (!layout_dirty_) return;
        layout_node(*root_, viewport_rect());
    }

    void layout_node_transactional(Node& node, Rect bounds) {
        checkpoint_layout_node(node);
        bounds.w = std::isfinite(bounds.w) ? std::max(0.0f, bounds.w) : 0.0f;
        bounds.h = std::isfinite(bounds.h) ? std::max(0.0f, bounds.h) : 0.0f;

        if (node.component->effective_visibility() == VisibilityMode::Collapsed) {
            node.bounds = Rect{bounds.x, bounds.y, 0.0f, 0.0f};
            for (auto& child : node.children) {
                layout_node_transactional(*child, node.bounds);
            }
            node.layout_dirty = false;
            return;
        }

        node.bounds = bounds;
        const auto constraints = Constraints::tight({bounds.w, bounds.h});
        const auto child_metrics = measure_children(node, constraints);

        std::vector<ChildPlacement> placements(node.children.size());
        node.component->layout_children(bounds, child_metrics, placements);

        for (std::size_t i = 0; i < node.children.size(); ++i) {
            layout_node_transactional(
                *node.children[i], sanitize_placement(placements[i].bounds, bounds));
        }
        node.layout_dirty = false;
    }

    std::vector<LayoutTransactionCheckpoint> layout_transaction_checkpoints_;
    bool layout_transaction_active_{};
