    // retained: all callbacks intentionally retained beyond their originating stack frame
    // resolve through one per-Tree weak lifetime token plus stable NodeId. Immediate
    // InputContext/FocusContext/LifecycleContext callbacks keep their existing stack-
    // confined implementation in tree_layout.inc.
    struct RetainedInvalidationLifetime {
        Tree* tree{};
    };

    [[nodiscard]] std::weak_ptr<RetainedInvalidationLifetime> retained_invalidation_weak() {
        if (!retained_invalidation_lifetime_) {
            retained_invalidation_lifetime_ = std::make_shared<RetainedInvalidationLifetime>();
            retained_invalidation_lifetime_->tree = this;
        }
        return retained_invalidation_lifetime_;
    }

    void retire_retained_invalidation_generation() noexcept {
        if (!retained_invalidation_lifetime_) return;
        retained_invalidation_lifetime_->tree = nullptr;
        retained_invalidation_lifetime_.reset();
    }

    static std::size_t retained_subtree_node_count(const Node& node) noexcept {
        std::size_t count = 1;
        for (const auto& child : node.children) count += retained_subtree_node_count(*child);
        return count;
    }

    [[nodiscard]] Node* retained_invalidation_node(NodeId id) noexcept {
        if (!root_ || id == kInvalidNodeId ||
            !std::binary_search(retained_invalidation_live_nodes_.begin(),
                                retained_invalidation_live_nodes_.end(), id)) {
            return nullptr;
        }
        return find_node_mutable(*root_, id);
    }

    [[nodiscard]] std::function<void()> paint_invalidator(NodeId id) {
        const auto weak = retained_invalidation_weak();
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_node_paint(*live);
        };
    }

    [[nodiscard]] std::function<void()> layout_invalidator(NodeId id) {
        const auto weak = retained_invalidation_weak();
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_layout_from(*live);
        };
    }

    [[nodiscard]] std::function<void()> focus_invalidator(NodeId id) {
        const auto weak = retained_invalidation_weak();
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->focus_structure_dirty_ = true;
            tree->invalidate_node_paint(*live);
        };
    }

    void invalidate_availability_from_retained(Node& node) {
        if (!root_) return;
        if (syncing_availability_) {
            availability_structure_dirty_ = true;
            return;
        }
        if (!availability_differs(*root_)) return;
        availability_structure_dirty_ = true;
        if (lifecycle_transition_ == LifecycleTransition::Refresh || semantic_operation_running_ != 0) {
            invalidate_node_paint(node);
            return;
        }
        if (!active_) {
            sync_availability_inactive();
            return;
        }
        if (active_platform_ && dispatch_depth_ == 0 &&
            lifecycle_transition_ != LifecycleTransition::Refresh && semantic_operation_running_ == 0) {
            sync_availability_structure(*active_platform_);
            return;
        }
        invalidate_node_paint(node);
    }

    [[nodiscard]] std::function<void()> availability_invalidator(NodeId id) {
        const auto weak = retained_invalidation_weak();
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_availability_from_retained(*live);
        };
    }

    static std::function<void()> retained_paint_invalidator_factory(void* owner, NodeId id) {
        return static_cast<Tree*>(owner)->paint_invalidator(id);
    }
    static std::function<void()> retained_focus_requester_factory(void* owner, NodeId id) {
        auto* tree = static_cast<Tree*>(owner);
        const auto weak = tree->retained_invalidation_weak();
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* live_tree = lifetime->tree;
            auto* live = live_tree->retained_invalidation_node(id);
            if (!live) return;
            live_tree->retained_focus_target_ = id;
            live_tree->retained_focus_request_ = true;
            live_tree->invalidate_node_paint(*live);
        };
    }

    static std::function<void()> retained_layout_invalidator_factory(void* owner, NodeId id) {
        return static_cast<Tree*>(owner)->layout_invalidator(id);
    }

    static std::function<void()> retained_focus_invalidator_factory(void* owner, NodeId id) {
        return static_cast<Tree*>(owner)->focus_invalidator(id);
    }

    static std::function<void()> retained_availability_invalidator_factory(void* owner, NodeId id) {
        return static_cast<Tree*>(owner)->availability_invalidator(id);
    }

    void register_dynamic_node(Node& node) {
        auto* source = dynamic_source(node);
        if (!source) return;

        const auto weak = retained_invalidation_weak();
        const auto owner = node.id;
        source->set_structure_invalidator([weak, owner] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            const auto* live = tree->retained_invalidation_node(owner);
            if (!live) return;
            // Revoke containing rasters before enqueue/storage can throw.
            tree->invalidate_raster_cache_ancestry(live);
            tree->queue_dynamic_mutation(owner);
        });

        auto keys = node.retained_dynamic_keys ? *node.retained_dynamic_keys
                                              : source->desired_keys();
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

        auto retained_keys = keys;
        if (auto* existing = dynamic_record(node.id)) {
            existing->keys = std::move(keys);
        } else {
            dynamic_records_.push_back(DynamicRecord{node.id, std::move(keys)});
        }
        node.retained_dynamic_keys = std::move(retained_keys);
    }



    // Declared last so its owning shared_ptr is destroyed before the retained tree's
    // earlier data members. External callbacks hold weak_ptr only; after Tree
    // destruction they cannot recover a Tree pointer or keep the Tree alive.
    std::vector<NodeId> retained_invalidation_live_nodes_;
    std::vector<NodeId> retained_checkpoint_nodes_;

    void refresh_retained_components() {
        if (lifecycle_transition_active() || dispatch_depth_ != 0 || syncing_availability_) return;
        if (!retained_checkpoint_nodes_.empty()) {
            const auto participants = retained_checkpoint_nodes_;
            LifecycleTransitionScope guard{*this, LifecycleTransition::Refresh, true};
            for (const auto id : participants) {
                if (auto* node = retained_invalidation_node(id)) node->component->retained_checkpoint();
            }
        }
        // A failed refresh can leave availability work after its last opted-in
        // node was removed. Drain this suffix even with an empty participant set.
        if (active_ && active_platform_) sync_availability_structure(*active_platform_);
        else if (!active_) sync_availability_inactive();
        if (active_ && active_platform_) {
            drain_descendant_actions(*active_platform_);
            sync_availability_structure(*active_platform_);
        }
    }
    std::shared_ptr<RetainedInvalidationLifetime> retained_invalidation_lifetime_;
