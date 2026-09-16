    struct DynamicRecord {
        NodeId owner{kInvalidNodeId};
        std::vector<std::string> keys;
    };

    [[nodiscard]] static detail::DynamicChildrenSource* dynamic_source(Node& node) noexcept {
        return dynamic_cast<detail::DynamicChildrenSource*>(node.component.get());
    }

    [[nodiscard]] static const detail::DynamicChildrenSource* dynamic_source(
        const Node& node) noexcept {
        return dynamic_cast<const detail::DynamicChildrenSource*>(node.component.get());
    }

    [[nodiscard]] static Node* find_node_mutable(Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (auto& child : node.children) {
            if (auto* result = find_node_mutable(*child, id)) return result;
        }
        return nullptr;
    }

    [[nodiscard]] static NodeId max_node_id(const Node& node) noexcept {
        NodeId result = node.id;
        for (const auto& child : node.children) {
            result = std::max(result, max_node_id(*child));
        }
        return result;
    }

    void ensure_runtime_node_ids() noexcept {
        if (runtime_node_ids_initialized_ || !root_) return;
        const auto maximum = max_node_id(*root_);
        next_node_id_ = maximum == std::numeric_limits<NodeId>::max()
            ? kInvalidNodeId
            : maximum + 1;
        runtime_node_ids_initialized_ = true;
    }

    [[nodiscard]] DynamicRecord* dynamic_record(NodeId owner) noexcept {
        for (auto& record : dynamic_records_) {
            if (record.owner == owner) return &record;
        }
        return nullptr;
    }

    [[nodiscard]] static bool contains_dynamic_key(
        const std::vector<std::string>& keys, const std::string& key) {
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    }

    [[nodiscard]] static bool has_duplicate_dynamic_keys(
        const std::vector<std::string>& keys) {
        for (std::size_t i = 0; i < keys.size(); ++i) {
            for (std::size_t j = i + 1; j < keys.size(); ++j) {
                if (keys[i] == keys[j]) return true;
            }
        }
        return false;
    }

    void register_dynamic_node(Node& node) {
        auto* source = dynamic_source(node);
        if (!source) return;

        source->set_structure_invalidator([this, owner = node.id] {
            queue_dynamic_mutation(owner);
        });

        auto keys = source->desired_keys();
        if (has_duplicate_dynamic_keys(keys)) {
            structural_diagnostic_ = "duplicate dynamic key";
            keys.clear();
        }
        if (keys.size() != node.children.size()) {
            structural_diagnostic_ = "dynamic initial child/key count mismatch";
            keys.clear();
            keys.reserve(node.children.size());
            for (std::size_t i = 0; i < node.children.size(); ++i) {
                keys.push_back("initial:" + std::to_string(i));
            }
        }

        if (auto* existing = dynamic_record(node.id)) {
            existing->keys = std::move(keys);
        } else {
            dynamic_records_.push_back(DynamicRecord{node.id, std::move(keys)});
        }
    }

    void unregister_dynamic_node(NodeId owner) noexcept {
        std::erase_if(dynamic_records_, [owner](const DynamicRecord& record) {
            return record.owner == owner;
        });
        std::erase(dirty_dynamic_nodes_, owner);
        if (dynamic_records_.empty()) {
            dynamic_focus_rebuild_pending_ = false;
            dynamic_focus_restore_ = kInvalidNodeId;
            dynamic_reconcile_retry_all_ = false;
        }
    }

    void queue_dynamic_mutation(NodeId owner) {
        if (!root_ || owner == kInvalidNodeId) return;

        // Capture the initial retained-tree high-water mark before the first
        // dynamic removal can destroy a node. Runtime NodeIds are monotonic for
        // the complete Tree lifetime and must never be recycled after removal.
        ensure_runtime_node_ids();

        ++structural_epoch_;
        if (std::find(dirty_dynamic_nodes_.begin(), dirty_dynamic_nodes_.end(), owner) ==
            dirty_dynamic_nodes_.end()) {
            try {
                dirty_dynamic_nodes_.push_back(owner);
            } catch (...) {
                // Exact owner bookkeeping can itself allocate. Preserve a
                // no-allocation conservative checkpoint before propagating so a
                // later safe flush can rediscover every live dynamic owner.
                dynamic_reconcile_retry_all_ = true;
                throw;
            }
        }

        // Observers only enqueue structural work. They must not claim that
        // layout changed before reconciliation has compared logical identity.
        // A paint invalidation is enough to request a normal UI checkpoint;
        // reconcile_dynamic_node() performs the actual layout invalidation only
        // when the retained key/branch structure changes.
        invalidate_paint(viewport_rect());
    }

    static void collect_dirty_dynamic_nodes(
        Node& node,
        const std::vector<NodeId>& captured,
        std::vector<NodeId>& ordered) {
        if (dynamic_source(node) &&
            std::find(captured.begin(), captured.end(), node.id) != captured.end()) {
            ordered.push_back(node.id);
        }
        for (auto& child : node.children) {
            collect_dirty_dynamic_nodes(*child, captured, ordered);
        }
    }

    static void collect_focusable_nodes(Node& node, std::vector<Node*>& result) {
        if (node.component->focusable()) result.push_back(&node);
        for (auto& child : node.children) collect_focusable_nodes(*child, result);
    }

    void maybe_throw_dynamic_reconcile_fault(int stage) {
        if (dynamic_reconcile_fault_stage_ != stage) return;
        dynamic_reconcile_fault_stage_ = 0;
        throw std::bad_alloc{};
    }

    [[nodiscard]] std::vector<Node*> prepare_focusables_after_structure() {
        // Test-only per-Tree fault seam. Production state stays instance-owned;
        // the zero value adds only a predictable branch and no allocation.
        maybe_throw_dynamic_reconcile_fault(3);
        std::vector<Node*> prepared;
        collect_focusable_nodes(*root_, prepared);
        return prepared;
    }

    void commit_focusables_after_structure(
        std::vector<Node*>& prepared,
        NodeId retained_focus_id) noexcept {
        focusables_.swap(prepared);
        dynamic_focus_rebuild_pending_ = false;
        dynamic_focus_restore_ = kInvalidNodeId;

        auto* retained_focus = retained_focus_id == kInvalidNodeId
            ? nullptr
            : find_node_mutable(*root_, retained_focus_id);
        if (!retained_focus) {
            focus_active_ = false;
            focused_index_ = 0;
            return;
        }

        const auto it = std::find(focusables_.begin(), focusables_.end(), retained_focus);
        if (it == focusables_.end()) {
            focus_active_ = false;
            focused_index_ = 0;
            return;
        }
        focused_index_ = static_cast<std::size_t>(std::distance(focusables_.begin(), it));
        focus_active_ = true;
    }

    void defer_focusables_after_structure(
        NodeId retained_focus_id,
        bool clear_current_registry) noexcept {
        // A failed post-commit registry rebuild cannot be allowed to leave a
        // partially populated vector or dangling pointers to removed nodes.
        // Record only stable identity and a boolean checkpoint; repair can then
        // allocate at the next safe reconciliation boundary.
        if (clear_current_registry) {
            focusables_.clear();
            focus_active_ = false;
            focused_index_ = 0;
        }
        dynamic_focus_restore_ = retained_focus_id;
        dynamic_focus_rebuild_pending_ = true;
        focus_structure_dirty_ = true;
    }

    [[nodiscard]] bool subtree_is_being_removed(
        const Node* candidate,
        const Node& owner,
        const std::vector<std::string>& current_keys,
        const std::vector<std::string>& desired_keys) const noexcept {
        if (!candidate) return false;
        const auto count = std::min(current_keys.size(), owner.children.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (!contains_dynamic_key(desired_keys, current_keys[i]) &&
                is_descendant_or_self(candidate, owner.children[i].get())) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] NodeId surviving_trapping_scope_for_removal(
        Node* focused,
        const Node& owner,
        const std::vector<std::string>& current_keys,
        const std::vector<std::string>& desired_keys) const noexcept {
        for (auto* current = focused; current; current = current->parent) {
            if (!current->component->effective_availability().interactive() ||
                !current->component->is_focus_scope() ||
                !current->component->focus_scope_active() ||
                !current->component->focus_scope_traps()) {
                continue;
            }
            if (!subtree_is_being_removed(current, owner, current_keys, desired_keys)) {
                return current->id;
            }
        }
        return kInvalidNodeId;
    }

    [[nodiscard]] NodeId removed_trapping_scope_restore_target_for_removal(
        Node* focused,
        const Node& owner,
        const std::vector<std::string>& current_keys,
        const std::vector<std::string>& desired_keys) const noexcept {
        for (auto* current = focused; current; current = current->parent) {
            if (!current->component->effective_availability().interactive() ||
                !current->component->is_focus_scope() ||
                !current->component->focus_scope_active() ||
                !current->component->focus_scope_traps() ||
                !subtree_is_being_removed(current, owner, current_keys, desired_keys)) {
                continue;
            }

            const auto restore_id = current->focus_restore;
            if (restore_id == kInvalidNodeId) continue;
            const auto* restore = find_node(*root_, restore_id);
            if (restore &&
                !subtree_is_being_removed(restore, owner, current_keys, desired_keys)) {
                return restore_id;
            }
        }
        return kInvalidNodeId;
    }

    void clear_removed_focus_restore_targets(
        Node& node,
        const Node& owner,
        const std::vector<std::string>& current_keys,
        const std::vector<std::string>& desired_keys) noexcept {
        if (node.focus_restore != kInvalidNodeId) {
            const auto* restore = find_node(*root_, node.focus_restore);
            if (!restore ||
                subtree_is_being_removed(restore, owner, current_keys, desired_keys)) {
                node.focus_restore = kInvalidNodeId;
            }
        }
        for (auto& child : node.children) {
            clear_removed_focus_restore_targets(
                *child, owner, current_keys, desired_keys);
        }
    }

    void reserve_dynamic_retry(NodeId owner) {
        if (std::find(dirty_dynamic_nodes_.begin(), dirty_dynamic_nodes_.end(), owner) ==
            dirty_dynamic_nodes_.end()) {
            dirty_dynamic_nodes_.push_back(owner);
        }
    }

    void mark_dynamic_structure_dirty(Node& owner) noexcept {
        for (auto* current = &owner; current; current = current->parent) {
            current->layout_dirty = true;
        }
        layout_dirty_ = true;
        focus_structure_dirty_ = true;
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

        // A previous structural commit may have completed while focus-registry
        // allocation failed. Repair that durable checkpoint before making any
        // further structural decision, even when the logical keys have already
        // reached their desired value.
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
            if (repaired_pending_focus) {
                std::erase(dirty_dynamic_nodes_, owner.id);
            }
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
                subtree_is_being_removed(
                    pointer_hover_target_, owner, current_keys, desired_keys)) {
                InputEvent leave{};
                leave.type = InputType::PointerLeave;
                (void)clear_pointer_hover(leave, *active_platform_);
            }

            if (auto* capture = pointer_capture_owner();
                subtree_is_being_removed(capture, owner, current_keys, desired_keys)) {
                (void)cancel_pointer_capture(*active_platform_);
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

            // Teardown callbacks may change the desired structure. Do not apply
            // a stale snapshot after arbitrary application code has run.
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

        // Reconciliation consumes the caller's captured dirty marker before it
        // invokes arbitrary lifecycle code. Reserve a replacement checkpoint
        // first so any exceptional exit after lifecycle mutation has durable
        // work for the next safe reconciliation checkpoint. Allocation can
        // still fail here, but no retained lifecycle state has changed yet.
        reserve_dynamic_retry(owner.id);
        const auto reconciliation_epoch = structural_epoch_;

        // Family closure: every allocation needed to publish the retained-only
        // removal checkpoint is prepared before the first teardown callback.
        // This includes string ownership for the DynamicRecord commit, not just
        // vector capacity. A failure here therefore leaves lifecycle/ownership
        // entirely at the old coherent state.
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

        Node* retained_focus = current_focus_node();
        const NodeId retained_focus_id = retained_focus ? retained_focus->id : kInvalidNodeId;
        clear_removed_focus_restore_targets(*root_, owner, current_keys, desired_keys);
        if (availability_cleared_focus_ &&
            subtree_is_being_removed(
                availability_cleared_focus_, owner, current_keys, desired_keys)) {
            availability_cleared_focus_ = nullptr;
        }

        // Removed siblings tear down in reverse retained order, matching normal
        // child-to-parent/reverse-sibling lifecycle semantics. A lifecycle hook
        // can fail only after the recursive helper has completed its own
        // best-effort traversal, so keep walking the remaining removed roots and
        // preserve the first failure for propagation after structural commit.
        std::exception_ptr removal_error;
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

        // All storage above is already prepared. From the first ownership move
        // through child/key publication there is no allocation and no callback.
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
                // The old focus vector may point at nodes still owned by
                // previous_children. Clear it before those nodes are destroyed,
                // then retain only stable identity for a later repair.
                removal_focus_error = std::current_exception();
                defer_focusables_after_structure(retained_focus_id, true);
            }
        }

        // At this point only removed, already-unmounted nodes remain in
        // previous_children. Destroy them before constructing replacement nodes
        // so destructor/lifetime ordering matches the retained-tree contract.
        previous_children.clear();

        if (removed_structure) mark_dynamic_structure_dirty(owner);

        if (removal_error) {
            std::rethrow_exception(removal_error);
        }
        if (removal_focus_error) {
            std::rethrow_exception(removal_focus_error);
        }

        // Removed lifecycle callbacks are arbitrary application code. If they
        // changed structure, the retained-only checkpoint above is the only
        // snapshot safe to publish from the stale pass. Leave the reserved
        // marker in place and let the next pass rebuild from the fresh source.
        if (structural_epoch_ != reconciliation_epoch) {
            return removed_structure;
        }

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
            initialize_availability(
                *inserted, owner.component->effective_availability());
            inserted_nodes.emplace_back(wanted.key, std::move(inserted));
        }

        // Resolve the complete child plan while owner.children is still intact.
        // Any internal mismatch therefore fails before retained ownership moves.
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

        // Progress bookkeeping is part of the prepare phase. These vectors may
        // allocate, so construct them before moving retained ownership out of
        // owner.children or publishing the desired key set. After this point the
        // insertion transaction itself adds no allocation solely for rollback.
        std::vector<bool> mount_started(desired_keys.size(), false);
        std::vector<bool> activation_started(desired_keys.size(), false);

        // DynamicRecord key publication is part of the same transaction. Copy
        // the fully-owned desired snapshot now; the commit below only swaps.
        maybe_throw_dynamic_reconcile_fault(2);
        auto prepared_desired_keys = desired_keys;

        // Provisional mount mutates the Tree-wide focus registry by appending
        // focusable nodes before the dynamic lifecycle transaction is committed.
        // The retained prefix is stable across this insertion attempt, so a size
        // checkpoint plus the prior focus cursor is sufficient for allocation-free
        // rollback across one or many inserted roots.
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

        // Exact insertion structural commit: both operations are no-throw with
        // the default allocators and all owning storage was prepared above.
        owner.children.swap(next_children);
        if (auto* refreshed = dynamic_record(owner.id)) {
            refreshed->keys.swap(prepared_desired_keys);
        }

        // Newly created nodes are attached before callbacks can inspect their
        // retained surroundings, but their lifecycle publication is still a
        // transaction. If mount/activate throws, unwind every inserted root
        // whose lifecycle started, detach all inserted roots, restore the
        // retained-only key checkpoint, and rethrow the original exception.
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
            // Remove every provisional focusable before rollback callbacks or
            // attempted-node destruction can expose a stale raw pointer. This
            // only shrinks the existing vector and cannot allocate or invoke user
            // code. Restore the retained focus cursor from the same checkpoint.
            while (focusables_.size() > retained_focusables_size) {
                focusables_.pop_back();
            }
            focus_active_ = retained_focus_active &&
                retained_focused_index < focusables_.size();
            focused_index_ = focus_active_ ? retained_focused_index : 0;

            for (std::size_t i = owner.children.size(); i-- > 0;) {
                if (!inserted_flags[i] || !mount_started[i]) continue;
                if (activation_started[i]) {
                    try {
                        deactivate_node(*owner.children[i]);
                    } catch (...) {
                        // Preserve the original insertion failure. The helper
                        // has already completed best-effort recursive teardown.
                    }
                }
                try {
                    unmount_node(*owner.children[i]);
                } catch (...) {
                    // Same first-error rule: rollback failures cannot replace
                    // the exception that caused the transaction to abort.
                }
            }

            // Recovery must not allocate after structural publication. Compact
            // retained children in-place inside the already-owned vector, then
            // swap that storage back to owner.children. The DynamicRecord swap
            // restores the retained-only key checkpoint without string copies.
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

        // Lifecycle is now committed. Build focus state transactionally in a
        // temporary vector; if allocation fails, keep the retained-only registry
        // (which contains only live pointers), record a stable repair target and
        // leave the reserved dynamic retry marker in place.
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
                    if (auto* restore =
                            find_node_mutable(*root_, removed_focus_restore_target);
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

        // If no callback queued structural work during this pass, the reserved
        // marker is only this transaction's exception checkpoint and can be
        // retired. Otherwise preserve it for the next bounded reconciliation
        // pass so callback-driven state is not lost.
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

        // This guard is part of the authoritative reconciliation state and must
        // never survive an exception. Recovery below performs only scalar stores;
        // it does not allocate or invoke application code.
        reconciling_dynamic_ = true;
        constexpr std::size_t kMaxStructuralPasses = 32;
        std::size_t pass = 0;

        try {
            while ((!dirty_dynamic_nodes_.empty() || dynamic_reconcile_retry_all_) &&
                   pass < kMaxStructuralPasses) {
                std::vector<NodeId> captured;

                if (dynamic_reconcile_retry_all_) {
                    // A previous throw may have occurred after the exact dirty
                    // list was moved/cleared, or before an owner could reserve
                    // its individual retry marker. Rebuild conservatively from
                    // live dynamic records. Keep the coarse marker set until the
                    // complete snapshot exists so allocation failure cannot lose
                    // the recovery state itself.
                    captured.reserve(dynamic_records_.size());
                    for (const auto& record : dynamic_records_) {
                        captured.push_back(record.owner);
                    }
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
        } catch (...) {
            // Exact captured/unprocessed/reentrant owner bookkeeping may no
            // longer be reconstructible without allocation. Preserve a
            // conservative per-Tree retry checkpoint and restore the guard
            // before the original exception crosses this boundary.
            dynamic_reconcile_retry_all_ = true;
            reconciling_dynamic_ = false;
            throw;
        }

        reconciling_dynamic_ = false;
        if (!dirty_dynamic_nodes_.empty() || dynamic_reconcile_retry_all_) {
            structural_diagnostic_ = "structural reconciliation pass limit exceeded";
            invalidate_paint(viewport_rect());
        }
    }

    std::vector<DynamicRecord> dynamic_records_;
    std::vector<NodeId> dirty_dynamic_nodes_;
    NodeId next_node_id_{1};
    NodeId dynamic_focus_restore_{kInvalidNodeId};
    std::uint64_t structural_epoch_{};
    int dynamic_reconcile_fault_stage_{};
    bool runtime_node_ids_initialized_{};
    bool reconciling_dynamic_{};
    bool dynamic_reconcile_retry_all_{};
    bool dynamic_focus_rebuild_pending_{};
    std::string structural_diagnostic_;
