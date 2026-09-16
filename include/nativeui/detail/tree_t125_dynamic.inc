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

    bool reconcile_dynamic_node(Node& owner) {
        std::vector<NodeId> quarantine;
        if (auto* source = dynamic_source(owner)) {
            if (auto* record = dynamic_record(owner.id);
                record && record->keys.size() == owner.children.size()) {
                auto desired_keys = source->desired_keys();
                if (!has_duplicate_dynamic_keys(desired_keys) && desired_keys != record->keys) {
                    quarantine.reserve(owner.children.size());
                    for (std::size_t i = 0; i < owner.children.size(); ++i) {
                        if (!contains_dynamic_key(desired_keys, record->keys[i])) {
                            quarantine.push_back(owner.children[i]->id);
                        }
                    }
                }
            }
        }

        if (quarantine.empty()) return reconcile_dynamic_node_t130_impl(owner);

        std::vector<NodeId> previous_quarantine;
        previous_quarantine.swap(dynamic_teardown_quarantine_ids_);
        dynamic_teardown_quarantine_ids_.swap(quarantine);
        auto restore_quarantine = [this, &previous_quarantine]() noexcept {
            dynamic_teardown_quarantine_ids_.swap(previous_quarantine);
        };

        try {
            const bool changed = reconcile_dynamic_node_t130_impl(owner);
            restore_quarantine();
            return changed;
        } catch (...) {
            restore_quarantine();
            throw;
        }
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

    std::vector<NodeId> dynamic_teardown_quarantine_ids_;
    bool fail_next_dynamic_enqueue_for_testing_{};
