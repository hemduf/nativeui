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
            dirty_dynamic_nodes_.push_back(owner);
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

    void rebuild_focusables_after_structure(Node* retained_focus) {
        focusables_.clear();
        collect_focusable_nodes(*root_, focusables_);
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

    bool reconcile_dynamic_node(Node& owner) {
        auto* source = dynamic_source(owner);
        auto* record = dynamic_record(owner.id);
        if (!source || !record) return false;

        auto desired_keys = source->desired_keys();
        if (has_duplicate_dynamic_keys(desired_keys)) {
            structural_diagnostic_ = "duplicate dynamic key";
            return false;
        }
        if (desired_keys == record->keys) return false;
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

        Node* retained_focus = current_focus_node();
        clear_removed_focus_restore_targets(*root_, owner, current_keys, desired_keys);
        if (availability_cleared_focus_ &&
            subtree_is_being_removed(
                availability_cleared_focus_, owner, current_keys, desired_keys)) {
            availability_cleared_focus_ = nullptr;
        }

        // Removed siblings tear down in reverse retained order, matching normal
        // child-to-parent/reverse-sibling lifecycle semantics.
        for (std::size_t i = owner.children.size(); i-- > 0;) {
            if (contains_dynamic_key(desired_keys, current_keys[i])) continue;
            if (active_) deactivate_node(*owner.children[i]);
            unmount_node(*owner.children[i]);
        }

        // Detach retained and removed nodes into separate ownership before any
        // replacement factory is invoked. The retained-only intermediate tree
        // is coherent, and removed nodes are destroyed before new nodes exist.
        auto previous_children = std::move(owner.children);
        std::vector<std::unique_ptr<Node>> retained_children;
        std::vector<std::string> retained_keys;
        retained_children.reserve(previous_children.size());
        retained_keys.reserve(previous_children.size());
        for (const auto& key : desired_keys) {
            const auto current = std::find(current_keys.begin(), current_keys.end(), key);
            if (current == current_keys.end()) continue;
            const auto index = static_cast<std::size_t>(
                std::distance(current_keys.begin(), current));
            if (previous_children[index]) {
                retained_children.push_back(std::move(previous_children[index]));
                retained_keys.push_back(key);
            }
        }

        owner.children = std::move(retained_children);
        if (auto* refreshed = dynamic_record(owner.id)) refreshed->keys = retained_keys;
        rebuild_focusables_after_structure(retained_focus);

        // At this point only removed, already-unmounted nodes remain in
        // previous_children. Destroy them before constructing replacement nodes
        // so destructor/lifetime ordering matches the retained-tree contract.
        previous_children.clear();

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

        std::vector<std::unique_ptr<Node>> next_children;
        std::vector<bool> inserted_flags;
        next_children.reserve(desired_keys.size());
        inserted_flags.reserve(desired_keys.size());
        auto retained = std::move(owner.children);
        for (const auto& key : desired_keys) {
            const auto retained_it = std::find(retained_keys.begin(), retained_keys.end(), key);
            if (retained_it != retained_keys.end()) {
                const auto index = static_cast<std::size_t>(
                    std::distance(retained_keys.begin(), retained_it));
                next_children.push_back(std::move(retained[index]));
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
            next_children.push_back(std::move(inserted_it->second));
            inserted_flags.push_back(true);
        }

        owner.children = std::move(next_children);
        if (auto* refreshed = dynamic_record(owner.id)) refreshed->keys = desired_keys;

        // Newly created nodes are now attached to the retained tree before
        // mount/activation callbacks can observe or mutate surrounding state.
        for (std::size_t i = 0; i < owner.children.size(); ++i) {
            if (!inserted_flags[i]) continue;
            mount_node(*owner.children[i]);
            if (active_) activate_node(*owner.children[i]);
        }

        rebuild_focusables_after_structure(retained_focus);
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
        return true;
    }

    void flush_dynamic_mutations() {
        if (reconciling_dynamic_ || !root_ || dirty_dynamic_nodes_.empty()) return;

        reconciling_dynamic_ = true;
        constexpr std::size_t kMaxStructuralPasses = 32;
        std::size_t pass = 0;

        while (!dirty_dynamic_nodes_.empty() && pass < kMaxStructuralPasses) {
            auto captured = std::move(dirty_dynamic_nodes_);
            dirty_dynamic_nodes_.clear();

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

        reconciling_dynamic_ = false;
        if (!dirty_dynamic_nodes_.empty()) {
            structural_diagnostic_ = "structural reconciliation pass limit exceeded";
            invalidate_paint(viewport_rect());
        }
    }

    std::vector<DynamicRecord> dynamic_records_;
    std::vector<NodeId> dirty_dynamic_nodes_;
    NodeId next_node_id_{1};
    std::uint64_t structural_epoch_{};
    bool runtime_node_ids_initialized_{};
    bool reconciling_dynamic_{};
    std::string structural_diagnostic_;