    [[nodiscard]] bool dynamic_teardown_quarantines(const Node* candidate) const noexcept {
        if (!candidate) return false;
        for (auto* current = candidate; current; current = current->parent) {
            for (const auto root_id : dynamic_teardown_quarantine_ids_) {
                if (root_id != kInvalidNodeId && current->id == root_id) return true;
            }
        }
        return false;
    }

    void queue_dynamic_mutation(NodeId owner) {
        if (fail_next_dynamic_enqueue_for_testing_ && root_ && owner != kInvalidNodeId &&
            std::find(dirty_dynamic_nodes_.begin(), dirty_dynamic_nodes_.end(), owner) ==
                dirty_dynamic_nodes_.end()) {
            fail_next_dynamic_enqueue_for_testing_ = false;
            ensure_runtime_node_ids();
            ++structural_epoch_;
            dynamic_reconcile_retry_all_ = true;
            throw std::bad_alloc{};
        }
        queue_dynamic_mutation_t130_impl(owner);
    }

    void flush_dynamic_mutations() {
        if (reconciling_dynamic_ || !root_ ||
            (dirty_dynamic_nodes_.empty() && !dynamic_reconcile_retry_all_)) {
            return;
        }

        // Structural removal must not invalidate a durable semantic cursor while
        // its application callback is still on the stack. Once callbacks settle,
        // finish only the unstarted suffix before consuming structural work.
        if (pointer_hover_transition_running_ != 0 ||
            focus_transition_running_ != 0 ||
            focus_semantic_running_ != 0) {
            return;
        }
        if (active_platform_) {
            resume_pointer_hover_transitions(*active_platform_);
            resume_focus_transitions(*active_platform_);
        }

        const bool previous_reconciling = reconciling_dynamic_;
        reconciling_dynamic_ = true;
        constexpr std::size_t kMaxStructuralPasses = 32;
        std::size_t pass = 0;

        try {
            while ((!dirty_dynamic_nodes_.empty() || dynamic_reconcile_retry_all_) &&
                   pass < kMaxStructuralPasses) {
                std::vector<NodeId> captured;
                if (dynamic_reconcile_retry_all_) {
                    captured.reserve(dynamic_records_.size());
                    for (const auto& record : dynamic_records_) captured.push_back(record.owner);
                    dynamic_reconcile_retry_all_ = false;
                    dirty_dynamic_nodes_.clear();
                } else {
                    captured = std::move(dirty_dynamic_nodes_);
                    dirty_dynamic_nodes_.clear();
                }

                std::vector<NodeId> ordered;
                ordered.reserve(captured.size());
                collect_dirty_dynamic_nodes(*root_, captured, ordered);
                for (const auto owner_id : ordered) {
                    if (auto* node = find_node_mutable(*root_, owner_id)) {
                        (void)reconcile_dynamic_node(*node);
                    }
                }
                ++pass;
            }
            reconciling_dynamic_ = previous_reconciling;
        } catch (...) {
            dynamic_reconcile_retry_all_ = true;
            reconciling_dynamic_ = previous_reconciling;
            throw;
        }

        if (dynamic_reconcile_retry_all_ || !dirty_dynamic_nodes_.empty()) {
            structural_diagnostic_ = "structural reconciliation pass limit exceeded";
            invalidate_paint(viewport_rect());
        }
    }

    bool fail_next_dynamic_enqueue_for_testing_{};
