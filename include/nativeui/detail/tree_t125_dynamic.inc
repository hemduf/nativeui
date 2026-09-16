    [[nodiscard]] bool dynamic_teardown_quarantines(const Node* candidate) const noexcept {
        if (!candidate) return false;
        for (const auto* root : dynamic_teardown_quarantine_roots_) {
            if (root && is_descendant_or_self(candidate, root)) return true;
        }
        return false;
    }

    bool reconcile_dynamic_node(Node& owner) {
        auto* source = dynamic_source(owner);
        auto* record = dynamic_record(owner.id);
        if (!source || !record) return false;

        auto desired_keys = source->desired_keys();
        if (has_duplicate_dynamic_keys(desired_keys)) {
            structural_diagnostic_ = "duplicate dynamic key";
            return false;
        }

        bool repaired_pending_focus = false;
        if (dynamic_focus_rebuild_pending_) {
            reserve_dynamic_retry(owner.id);
            const auto restore_id = dynamic_focus_restore_;
            auto prepared_focusables = prepare_focusables_after_structure();
            commit_focusables_after_structure(prepared_focusables, restore_id);
            focus_structure_dirty_ = true;
            repaired_pending_focus = true;
        }

        if (desired_keys == record->keys) {
            if (repaired_pending_focus) std::erase(dirty_dynamic_nodes_, owner.id);
            return false;
        }
        if (record->keys.size() != owner.children.size()) {
            structural_diagnostic_ = "dynamic retained child/key count mismatch";
            return false;
        }

        const auto current_keys = record->keys;
        const auto teardown_epoch = structural_epoch_;
        bool removed_focus = false;
        NodeId removed_focus_restore_target{kInvalidNodeId};
        NodeId surviving_focus_scope{kInvalidNodeId};

        if (active_ && active_platform_) {
            if (pointer_hover_target_ &&
                subtree_is_being_removed(pointer_hover_target_, owner, current_keys, desired_keys)) {
                InputEvent leave{};
                leave.type = InputType::PointerLeave;
                (void)clear_pointer_hover(leave, *active_platform_);
            }

            if (auto* capture = pointer_capture_owner();
                subtree_is_being_removed(capture, owner, current_keys, desired_keys)) {
                try {
                    (void)cancel_pointer_capture(*active_platform_);
                } catch (...) {
                    pointer_interaction_active_ = false;
                    throw;
                }
                pointer_interaction_active_ = false;
            }

            if (auto* focused = current_focus_node();
                subtree_is_being_removed(focused, owner, current_keys, desired_keys)) {
                removed_focus_restore_target =
                    removed_trapping_scope_restore_target_for_removal(
                        focused, owner, current_keys, desired_keys);
                surviving_focus_scope = surviving_trapping_scope_for_removal(
                    focused, owner, current_keys, desired_keys);
                removed_focus = true;
                clear_focus(*active_platform_);
            }

            if (structural_epoch_ != teardown_epoch) {
                queue_dynamic_mutation(owner.id);
                return false;
            }
        }

        auto desired = source->desired_children();
        if (desired.size() != desired_keys.size()) {
            structural_diagnostic_ = "dynamic desired child/key count mismatch";
            return false;
        }
        for (std::size_t i = 0; i < desired.size(); ++i) {
            if (desired[i].key != desired_keys[i]) {
                structural_diagnostic_ = "dynamic desired key snapshot mismatch";
                return false;
            }
        }

        reserve_dynamic_retry(owner.id);
        const auto reconciliation_epoch = structural_epoch_;

        maybe_throw_dynamic_reconcile_fault(1);
        std::vector<std::size_t> retained_indices;
        std::vector<std::string> retained_keys;
        retained_indices.reserve(desired_keys.size());
        retained_keys.reserve(desired_keys.size());
        for (const auto& key : desired_keys) {
            const auto current = std::find(current_keys.begin(), current_keys.end(), key);
            if (current == current_keys.end()) continue;
            retained_indices.push_back(static_cast<std::size_t>(
                std::distance(current_keys.begin(), current)));
            retained_keys.push_back(key);
        }
        auto retained_keys_for_publish = retained_keys;
        std::vector<std::unique_ptr<Node>> retained_children;
        retained_children.reserve(retained_indices.size());
        const bool removed_structure = retained_indices.size() != current_keys.size();

        std::vector<Node*> teardown_roots;
        teardown_roots.reserve(owner.children.size());
        for (std::size_t i = 0; i < owner.children.size(); ++i) {
            if (!contains_dynamic_key(desired_keys, current_keys[i])) {
                teardown_roots.push_back(owner.children[i].get());
            }
        }

        clear_removed_focus_restore_targets(*root_, owner, current_keys, desired_keys);
        if (availability_cleared_focus_ &&
            subtree_is_being_removed(
                availability_cleared_focus_, owner, current_keys, desired_keys)) {
            availability_cleared_focus_ = nullptr;
        }

        std::exception_ptr removal_error;
        NodeId retained_focus_id{kInvalidNodeId};

        if (!teardown_roots.empty()) {
            std::vector<Node*> previous_quarantine;
            previous_quarantine.swap(dynamic_teardown_quarantine_roots_);
            dynamic_teardown_quarantine_roots_.swap(teardown_roots);
            auto restore_quarantine = [this, &previous_quarantine]() noexcept {
                dynamic_teardown_quarantine_roots_.swap(previous_quarantine);
            };

            try {
                if (active_ && active_platform_) {
                    resume_pointer_hover_transitions(*active_platform_);
                    resume_focus_transitions(*active_platform_);

                    if (dynamic_teardown_quarantines(pointer_hover_target_)) {
                        InputEvent leave{};
                        leave.type = InputType::PointerLeave;
                        (void)clear_pointer_hover(leave, *active_platform_);
                    }

                    if (auto* capture = pointer_capture_owner();
                        dynamic_teardown_quarantines(capture)) {
                        try {
                            (void)cancel_pointer_capture(*active_platform_);
                        } catch (...) {
                            pointer_interaction_active_ = false;
                            throw;
                        }
                        pointer_interaction_active_ = false;
                    }

                    if (auto* focused = current_focus_node();
                        dynamic_teardown_quarantines(focused)) {
                        if (!removed_focus) {
                            removed_focus_restore_target =
                                removed_trapping_scope_restore_target_for_removal(
                                    focused, owner, current_keys, desired_keys);
                            surviving_focus_scope = surviving_trapping_scope_for_removal(
                                focused, owner, current_keys, desired_keys);
                            removed_focus = true;
                        }
                        clear_focus(*active_platform_);
                    }

                    if (availability_cleared_focus_ &&
                        dynamic_teardown_quarantines(availability_cleared_focus_)) {
                        availability_cleared_focus_ = nullptr;
                    }
                }

                for (std::size_t i = owner.children.size(); i-- > 0;) {
                    if (contains_dynamic_key(desired_keys, current_keys[i])) continue;
                    if (active_) {
                        try {
                            deactivate_node(*owner.children[i]);
                        } catch (...) {
                            if (!removal_error) removal_error = std::current_exception();
                        }
                    }
                    try {
                        unmount_node(*owner.children[i]);
                    } catch (...) {
                        if (!removal_error) removal_error = std::current_exception();
                    }
                }

                // T125 semantic authority: lifecycle callbacks may move focus to
                // a surviving node. Capture the authoritative target only after
                // teardown callbacks have finished and before callback-free detach.
                if (auto* retained_focus = current_focus_node()) {
                    retained_focus_id = retained_focus->id;
                }
            } catch (...) {
                restore_quarantine();
                throw;
            }
            restore_quarantine();
        } else if (auto* retained_focus = current_focus_node()) {
            retained_focus_id = retained_focus->id;
        }

        auto previous_children = std::move(owner.children);
        for (const auto index : retained_indices) {
            if (previous_children[index]) {
                retained_children.push_back(std::move(previous_children[index]));
            }
        }
        owner.children.swap(retained_children);
        if (auto* refreshed = dynamic_record(owner.id)) {
            refreshed->keys.swap(retained_keys_for_publish);
        }

        std::exception_ptr removal_focus_error;
        if (removed_structure) {
            try {
                auto prepared_focusables = prepare_focusables_after_structure();
                commit_focusables_after_structure(prepared_focusables, retained_focus_id);
            } catch (...) {
                removal_focus_error = std::current_exception();
                defer_focusables_after_structure(retained_focus_id, true);
            }
        }

        previous_children.clear();
        if (removed_structure) mark_dynamic_structure_dirty(owner);

        if (removal_error) std::rethrow_exception(removal_error);
        if (removal_focus_error) std::rethrow_exception(removal_focus_error);
        if (structural_epoch_ != reconciliation_epoch) return removed_structure;

        ensure_runtime_node_ids();
        std::vector<std::pair<std::string, std::unique_ptr<Node>>> inserted_nodes;
        inserted_nodes.reserve(desired.size());
        for (auto& wanted : desired) {
            if (contains_dynamic_key(retained_keys, wanted.key)) continue;
            if (next_node_id_ == kInvalidNodeId) {
                structural_diagnostic_ = "dynamic NodeId space exhausted";
                queue_dynamic_mutation(owner.id);
                return false;
            }
            auto inserted = detail::compile_node(
                std::move(wanted.spec), next_node_id_, &owner);
            initialize_availability(*inserted, owner.component->effective_availability());
            inserted_nodes.emplace_back(wanted.key, std::move(inserted));
        }

        std::vector<std::size_t> child_indices;
        std::vector<bool> inserted_flags;
        child_indices.reserve(desired_keys.size());
        inserted_flags.reserve(desired_keys.size());
        for (const auto& key : desired_keys) {
            const auto retained_it = std::find(retained_keys.begin(), retained_keys.end(), key);
            if (retained_it != retained_keys.end()) {
                child_indices.push_back(static_cast<std::size_t>(
                    std::distance(retained_keys.begin(), retained_it)));
                inserted_flags.push_back(false);
                continue;
            }

            const auto inserted_it = std::find_if(
                inserted_nodes.begin(), inserted_nodes.end(),
                [&](const auto& entry) { return entry.first == key; });
            if (inserted_it == inserted_nodes.end() || !inserted_it->second) {
                structural_diagnostic_ = "dynamic inserted child/key mismatch";
                queue_dynamic_mutation(owner.id);
                return false;
            }
            child_indices.push_back(static_cast<std::size_t>(
                std::distance(inserted_nodes.begin(), inserted_it)));
            inserted_flags.push_back(true);
        }

        std::vector<std::unique_ptr<Node>> next_children;
        next_children.reserve(desired_keys.size());
        std::vector<bool> mount_started(desired_keys.size(), false);
        std::vector<bool> activation_started(desired_keys.size(), false);

        maybe_throw_dynamic_reconcile_fault(2);
        auto prepared_desired_keys = desired_keys;

        const auto retained_focusables_size = focusables_.size();
        const bool retained_focus_active = focus_active_;
        const auto retained_focused_index = focused_index_;

        auto retained = std::move(owner.children);
        for (std::size_t i = 0; i < child_indices.size(); ++i) {
            if (inserted_flags[i]) {
                next_children.push_back(std::move(inserted_nodes[child_indices[i]].second));
            } else {
                next_children.push_back(std::move(retained[child_indices[i]]));
            }
        }

        owner.children.swap(next_children);
        if (auto* refreshed = dynamic_record(owner.id)) {
            refreshed->keys.swap(prepared_desired_keys);
        }

        std::exception_ptr insertion_error;
        for (std::size_t i = 0; i < owner.children.size(); ++i) {
            if (!inserted_flags[i]) continue;
            mount_started[i] = true;
            try {
                mount_node(*owner.children[i]);
            } catch (...) {
                insertion_error = std::current_exception();
                break;
            }
            if (active_) {
                activation_started[i] = true;
                try {
                    activate_node(*owner.children[i]);
                } catch (...) {
                    insertion_error = std::current_exception();
                    break;
                }
            }
        }

        if (insertion_error) {
            while (focusables_.size() > retained_focusables_size) focusables_.pop_back();
            focus_active_ = retained_focus_active &&
                retained_focused_index < focusables_.size();
            focused_index_ = focus_active_ ? retained_focused_index : 0;

            for (std::size_t i = owner.children.size(); i-- > 0;) {
                if (!inserted_flags[i] || !mount_started[i]) continue;
                if (activation_started[i]) {
                    try {
                        deactivate_node(*owner.children[i]);
                    } catch (...) {
                    }
                }
                try {
                    unmount_node(*owner.children[i]);
                } catch (...) {
                }
            }

            auto attempted_children = std::move(owner.children);
            std::size_t retained_count = 0;
            for (std::size_t i = 0; i < attempted_children.size(); ++i) {
                if (inserted_flags[i] || !attempted_children[i]) continue;
                if (retained_count != i) {
                    attempted_children[retained_count] = std::move(attempted_children[i]);
                }
                ++retained_count;
            }
            attempted_children.resize(retained_count);
            owner.children.swap(attempted_children);
            if (auto* refreshed = dynamic_record(owner.id)) {
                refreshed->keys.swap(prepared_desired_keys);
            }

            mark_dynamic_structure_dirty(owner);
            std::rethrow_exception(insertion_error);
        }

        try {
            auto prepared_focusables = prepare_focusables_after_structure();
            commit_focusables_after_structure(prepared_focusables, retained_focus_id);
        } catch (...) {
            defer_focusables_after_structure(retained_focus_id, false);
            mark_dynamic_structure_dirty(owner);
            throw;
        }

        focus_structure_dirty_ = true;
        invalidate_layout_from(owner);

        if (active_ && active_platform_) {
            sync_focus_structure(*active_platform_);
            if (removed_focus && !focus_active_) {
                Node* fallback = nullptr;
                if (removed_focus_restore_target != kInvalidNodeId) {
                    if (auto* restore = find_node_mutable(*root_, removed_focus_restore_target);
                        focus_target_available(restore)) {
                        fallback = restore;
                    }
                }
                if (!fallback && surviving_focus_scope != kInvalidNodeId) {
                    if (auto* scope = find_node_mutable(*root_, surviving_focus_scope)) {
                        fallback = first_available_focusable(scope);
                    }
                }
                if (!fallback) fallback = first_available_focusable();
                if (fallback) set_focus(fallback, *active_platform_);
            }
        }

        if (structural_epoch_ == reconciliation_epoch) {
            std::erase(dirty_dynamic_nodes_, owner.id);
        }
        return true;
    }

    void flush_dynamic_mutations() {
        if (reconciling_dynamic_ || !root_ ||
            (dirty_dynamic_nodes_.empty() && !dynamic_reconcile_retry_all_)) {
            return;
        }

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

                for (const auto owner : ordered) {
                    if (auto* node = find_node_mutable(*root_, owner)) {
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

        if (!dirty_dynamic_nodes_.empty() || dynamic_reconcile_retry_all_) {
            structural_diagnostic_ = "structural reconciliation pass limit exceeded";
            invalidate_paint(viewport_rect());
        }
    }

    std::vector<Node*> dynamic_teardown_quarantine_roots_;
