    struct LayoutTransactionCheckpoint {
        Node* node{};
        Rect bounds{};
    };

    // Layout keeps the common checkpoint prefix on the transaction stack while
    // reusing only overflow storage across completed transactions. The retained
    // vector is per Tree, so a fixed >64-node UI pays overflow growth while
    // establishing its high-water mark (normally during warmup), not on every
    // successful dirty layout. This avoids both a successful-path subtree
    // pre-count and process/global scratch state.
    class LayoutTransactionJournal {
    public:
        static constexpr std::size_t kInlineCapacity = 64;

        explicit LayoutTransactionJournal(
            std::vector<LayoutTransactionCheckpoint>& overflow) noexcept
            : overflow_(overflow) {}

        void clear() noexcept {
            size_ = 0;
            overflow_.clear();
        }

        void push_back(LayoutTransactionCheckpoint value) {
            if (size_ < kInlineCapacity) {
                inline_[size_] = std::move(value);
            } else {
                // Allocation, when a Tree exceeds its previous high-water mark,
                // happens before the corresponding node mutation. vector's
                // strong allocation-failure guarantee leaves the already-recorded
                // rollback prefix intact.
                overflow_.push_back(std::move(value));
            }
            ++size_;
        }

        [[nodiscard]] LayoutTransactionCheckpoint& operator[](std::size_t index) noexcept {
            return index < kInlineCapacity ? inline_[index] : overflow_[index - kInlineCapacity];
        }

        [[nodiscard]] const LayoutTransactionCheckpoint& operator[](
            std::size_t index) const noexcept {
            return index < kInlineCapacity ? inline_[index] : overflow_[index - kInlineCapacity];
        }

        [[nodiscard]] std::size_t size() const noexcept { return size_; }

    private:
        LayoutTransactionCheckpoint inline_[kInlineCapacity]{};
        std::vector<LayoutTransactionCheckpoint>& overflow_;
        std::size_t size_{};
    };

    void begin_layout_geometry_transaction(LayoutTransactionJournal& journal) {
        // Retained layout callbacks are arbitrary application code. A nested
        // layout attempt would otherwise overwrite the rollback journal of the
        // outer attempt, so fail before mutating geometry and let the outer
        // transaction restore its checkpoint deterministically.
        if (layout_transaction_active_) {
            throw std::logic_error("reentrant retained layout transaction");
        }
        journal.clear();
        // Do not pre-count the subtree: that would add a full O(N) pass to every
        // successful dirty layout. The common <=64-node prefix is stack-backed;
        // larger trees spill into per-Tree storage whose capacity survives this
        // transaction. New high-water growth still happens before node mutation,
        // so an allocation failure leaves the recorded rollback prefix usable.
        layout_transaction_journal_ = &journal;
        layout_transaction_active_ = true;
        layout_dirty_ = true;
    }

    void checkpoint_layout_node(Node& node) {
        if (!layout_transaction_active_ || !layout_transaction_journal_) {
            throw std::logic_error("layout checkpoint outside transaction");
        }
        // Checkpoint immediately before mutation. Overflow growth, when needed,
        // can fail here before this node is changed; prior checkpoints remain
        // available to restore every earlier mutation in the transaction.
        layout_transaction_journal_->push_back(LayoutTransactionCheckpoint{&node, node.bounds});
    }

    void commit_layout_geometry_transaction() noexcept {
        if (layout_transaction_journal_) layout_transaction_journal_->clear();
        layout_transaction_journal_ = nullptr;
        layout_transaction_active_ = false;
        layout_dirty_ = false;
    }

    void rollback_layout_geometry_transaction() noexcept {
        if (layout_transaction_journal_) {
            for (std::size_t index = layout_transaction_journal_->size(); index-- > 0;) {
                auto& checkpoint = (*layout_transaction_journal_)[index];
                checkpoint.node->bounds = checkpoint.bounds;
                // A failed pass must force a complete retry even when this node
                // was clean before the attempt. Tree layout is whole-subtree
                // today, so marking every visited node dirty is conservative.
                checkpoint.node->layout_dirty = true;
            }
            layout_transaction_journal_->clear();
        }
        layout_transaction_journal_ = nullptr;
        layout_transaction_active_ = false;
        layout_dirty_ = true;
    }

    struct LayoutPublicationCheckpoint {
        Size viewport{};
        DirtyRegion paint_dirty{};
        LayoutTransactionJournal journal;

        LayoutPublicationCheckpoint(Tree& owner, Size next_viewport)
            : viewport(owner.viewport_),
              paint_dirty(std::move(owner.paint_dirty_)),
              journal(owner.layout_transaction_overflow_) {
            try {
                owner.begin_layout_geometry_transaction(journal);
            } catch (...) {
                owner.paint_dirty_ = std::move(paint_dirty);
                throw;
            }
            owner.active_layout_publication_ = this;
            owner.viewport_ = next_viewport;
            // paint_dirty_ is moved-from here. Layout callbacks may accumulate a
            // fresh region; commit restores the old storage before clearing it so
            // successful publication reuses capacity rather than allocating a
            // checkpoint copy on every fresh Tree layout.
            owner.paint_dirty_.clear();
        }

        LayoutPublicationCheckpoint(const LayoutPublicationCheckpoint&) = delete;
        LayoutPublicationCheckpoint& operator=(const LayoutPublicationCheckpoint&) = delete;
        LayoutPublicationCheckpoint(LayoutPublicationCheckpoint&&) = delete;
        LayoutPublicationCheckpoint& operator=(LayoutPublicationCheckpoint&&) = delete;
    };

    [[nodiscard]] LayoutPublicationCheckpoint begin_layout_publication(Size viewport) {
        // C++17 guaranteed copy elision constructs the checkpoint directly in
        // the caller, keeping the active journal pointer stable without a heap.
        return LayoutPublicationCheckpoint{*this, viewport};
    }

    void rollback_layout_publication(LayoutPublicationCheckpoint&& checkpoint) noexcept {
        rollback_layout_geometry_transaction();
        active_layout_publication_ = nullptr;
        viewport_ = checkpoint.viewport;
        paint_dirty_ = std::move(checkpoint.paint_dirty);
    }

    void commit_layout_publication() {
        // Reclaim the pre-transaction DirtyRegion storage before publishing the
        // fresh full-viewport invalidation. This preserves successful semantics
        // while avoiding a fourth fresh-Tree allocation in T052 multi_instance.
        auto* checkpoint = active_layout_publication_;
        commit_layout_geometry_transaction();
        active_layout_publication_ = nullptr;
        if (checkpoint) paint_dirty_ = std::move(checkpoint->paint_dirty);
        paint_dirty_.clear();
        invalidate_paint(viewport_rect());
    }

    void layout_node(Node& node, Rect bounds) {
        if (layout_transaction_active_) {
            layout_node_transactional(node, bounds);
            return;
        }

        LayoutTransactionJournal journal{layout_transaction_overflow_};
        begin_layout_geometry_transaction(journal);
        try {
            layout_node_transactional(node, bounds);
        } catch (...) {
            rollback_layout_geometry_transaction();
            throw;
        }
        commit_layout_geometry_transaction();
    }

    void ensure_layout() {
        if (!layout_dirty_) return;
        // ensure_layout() is a top-level lazy entry used by paint/input paths.
        // Recursive retained traversal stays inside layout_node_transactional();
        // a callback that re-enters paint while layout is active must not join
        // the outer journal and recursively invoke the same application layout.
        if (layout_transaction_active_) {
            throw std::logic_error("reentrant retained layout transaction");
        }
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

    // Keep only overflow capacity per Tree. The 64-entry fast prefix remains on
    // the transaction stack, avoiding a large permanent Tree-size increase while
    // preserving high-water reuse across repeated layouts.
    std::vector<LayoutTransactionCheckpoint> layout_transaction_overflow_;
    LayoutTransactionJournal* layout_transaction_journal_{};
    LayoutPublicationCheckpoint* active_layout_publication_{};
    bool layout_transaction_active_{};