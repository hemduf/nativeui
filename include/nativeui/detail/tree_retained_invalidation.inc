    // T129: all callbacks intentionally retained beyond their originating stack frame
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
        if (!active_) {
            sync_availability_inactive();
            return;
        }
        if (active_platform_ && dispatch_depth_ == 0) {
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
            if (!tree->retained_invalidation_node(owner)) return;
            tree->queue_dynamic_mutation(owner);
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

    void mount_node(Node& node) {
        if (!node.parent && retained_invalidation_live_nodes_.empty()) {
            retained_invalidation_live_nodes_.reserve(retained_subtree_node_count(node));
        }
        // Dynamic nodes receive monotonic ids, but their retained tree position can
        // precede older siblings. A later whole-Tree remount therefore does not
        // necessarily visit ids in sorted order. Keep the membership vector sorted
        // explicitly so binary_search/lower_bound remain valid across that remount.
        const auto live = std::lower_bound(retained_invalidation_live_nodes_.begin(),
                                           retained_invalidation_live_nodes_.end(), node.id);
        if (live == retained_invalidation_live_nodes_.end() || *live != node.id) {
            retained_invalidation_live_nodes_.insert(live, node.id);
        }

        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->bind_theme(node.parent ? inherited_theme_for(node) : theme_);
            const auto weak = retained_invalidation_weak();
            const auto id = node.id;
            binding->set_theme_change_invalidator(
                [weak, id](ThemeInvalidation invalidation) {
                    const auto lifetime = weak.lock();
                    if (!lifetime || !lifetime->tree) return;
                    auto* tree = lifetime->tree;
                    if (!tree->retained_invalidation_node(id)) return;
                    tree->apply_scoped_theme_change(id, invalidation);
                });
        }

        if (dynamic_source(node)) register_dynamic_node(node);
        MountContext context{
            node.id,
            MountContext::InvalidatorFactory{
                this,
                &Tree::retained_paint_invalidator_factory,
                &Tree::retained_layout_invalidator_factory,
                &Tree::retained_focus_invalidator_factory,
                &Tree::retained_availability_invalidator_factory},
            overlay_service_};
        node.component->mount(context);
        if (node.component->focusable()) focusables_.push_back(&node);
        for (auto& child : node.children) mount_node(*child);
    }

    void unmount_node(Node& node) {
        // Explicit whole-Tree unmount is a retained-callback generation boundary.
        // Rotate the token before the first arbitrary child/component teardown
        // callback so callbacks issued by the previous mount can never become live
        // again if the same retained nodes/NodeIds are mounted later.
        if (!node.parent) retire_retained_invalidation_generation();

        // Terminal retained removal begins before descendant teardown. Retire this
        // node's long-lived identity first so arbitrary child unmount callbacks
        // cannot re-enter paint/layout/focus/availability/theme/dynamic invalidation
        // through an ancestor whose subtree is already being removed.
        const auto live = std::lower_bound(retained_invalidation_live_nodes_.begin(),
                                           retained_invalidation_live_nodes_.end(), node.id);
        if (live != retained_invalidation_live_nodes_.end() && *live == node.id) {
            retained_invalidation_live_nodes_.erase(live);
        }

        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            unmount_node(**it);
        }

        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->set_theme_change_invalidator({});
        }
        if (auto* source = dynamic_source(node)) {
            source->set_structure_invalidator({});
            unregister_dynamic_node(node.id);
        }

        LifecycleContext context{
            node.id, node.bounds, unsafe_paint_invalidator(node), unsafe_layout_invalidator(node)};
        node.component->unmount(context);
    }

    // Declared last so its owning shared_ptr is destroyed before the retained tree's
    // earlier data members. External callbacks hold weak_ptr only; after Tree
    // destruction they cannot recover a Tree pointer or keep the Tree alive.
    std::vector<NodeId> retained_invalidation_live_nodes_;
    std::shared_ptr<RetainedInvalidationLifetime> retained_invalidation_lifetime_;