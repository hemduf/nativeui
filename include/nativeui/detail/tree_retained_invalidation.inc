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

    [[nodiscard]] Node* retained_invalidation_node(NodeId id) noexcept {
        if (!root_ || id == kInvalidNodeId || !retained_invalidation_live_nodes_.contains(id)) {
            return nullptr;
        }
        return find_node_mutable(*root_, id);
    }

    [[nodiscard]] std::function<void()> paint_invalidator(Node& node) {
        const auto weak = retained_invalidation_weak();
        const auto id = node.id;
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_node_paint(*live);
        };
    }

    [[nodiscard]] std::function<void()> layout_invalidator(Node& node) {
        const auto weak = retained_invalidation_weak();
        const auto id = node.id;
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_layout_from(*live);
        };
    }

    [[nodiscard]] std::function<void()> focus_invalidator(Node& node) {
        const auto weak = retained_invalidation_weak();
        const auto id = node.id;
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

    [[nodiscard]] std::function<void()> availability_invalidator(Node& node) {
        const auto weak = retained_invalidation_weak();
        const auto id = node.id;
        return [weak, id] {
            const auto lifetime = weak.lock();
            if (!lifetime || !lifetime->tree) return;
            auto* tree = lifetime->tree;
            auto* live = tree->retained_invalidation_node(id);
            if (!live) return;
            tree->invalidate_availability_from_retained(*live);
        };
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
        retained_invalidation_live_nodes_.insert(node.id);
        const auto weak = retained_invalidation_weak();

        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->bind_theme(node.parent ? inherited_theme_for(node) : theme_);
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
            paint_invalidator(node),
            layout_invalidator(node),
            focus_invalidator(node),
            availability_invalidator(node),
            overlay_service_};
        node.component->mount(context);
        if (node.component->focusable()) focusables_.push_back(&node);
        for (auto& child : node.children) mount_node(*child);
    }

    void unmount_node(Node& node) {
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            unmount_node(**it);
        }

        // Terminal retirement precedes arbitrary component teardown callbacks. Any
        // previously-issued retained invalidator is therefore already a no-op even
        // while the Node object still occupies its old address.
        retained_invalidation_live_nodes_.erase(node.id);
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
    std::unordered_set<NodeId> retained_invalidation_live_nodes_;
    std::shared_ptr<RetainedInvalidationLifetime> retained_invalidation_lifetime_;
