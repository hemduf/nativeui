    // Stack-confined lifecycle/focus call sites keep the immediate invalidator
    // contract from tree_layout.inc while T129 retained callbacks use the NodeId
    // overloads/factories from tree_retained_invalidation.inc.
    [[nodiscard]] std::function<void()> paint_invalidator(Node& node) {
        return unsafe_paint_invalidator(node);
    }

    [[nodiscard]] std::function<void()> layout_invalidator(Node& node) {
        return unsafe_layout_invalidator(node);
    }

    struct MountTransitionProgress {
        Node* node{};
        bool live_identity_started{};
        bool theme_setup_started{};
        bool dynamic_setup_started{};
        bool component_started{};
    };

    [[nodiscard]] bool retain_mount_identity(Node& node) {
        if (!node.parent && retained_invalidation_live_nodes_.empty()) {
            retained_invalidation_live_nodes_.reserve(retained_subtree_node_count(node));
        }
        const auto live = std::lower_bound(retained_invalidation_live_nodes_.begin(),
                                           retained_invalidation_live_nodes_.end(), node.id);
        if (live != retained_invalidation_live_nodes_.end() && *live == node.id) return false;
        retained_invalidation_live_nodes_.insert(live, node.id);
        return true;
    }

    void retire_mount_identity(NodeId id) noexcept {
        const auto live = std::lower_bound(retained_invalidation_live_nodes_.begin(),
                                           retained_invalidation_live_nodes_.end(), id);
        if (live != retained_invalidation_live_nodes_.end() && *live == id) {
            retained_invalidation_live_nodes_.erase(live);
        }
    }

    void retire_subtree_identities(Node& node) noexcept {
        retire_mount_identity(node.id);
        for (auto& child : node.children) retire_subtree_identities(*child);
    }

    void mount_node_recorded(Node& node) {
        // Journal before the first fallible operation. Allocation failure therefore
        // occurs before this node acquires retained state; every later failure has
        // an exact rollback record.
        mount_transition_progress_.push_back(MountTransitionProgress{&node});
        auto& progress = mount_transition_progress_.back();

        progress.live_identity_started = retain_mount_identity(node);

        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            progress.theme_setup_started = true;
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
        if (dynamic_source(node)) {
            progress.dynamic_setup_started = true;
            register_dynamic_node(node);
        }

        MountContext context{
            node.id,
            MountContext::InvalidatorFactory{
                this,
                &Tree::retained_paint_invalidator_factory,
                &Tree::retained_layout_invalidator_factory,
                &Tree::retained_focus_invalidator_factory,
                &Tree::retained_availability_invalidator_factory},
            overlay_service_};
        progress.component_started = true;
        node.component->mount(context);
        if (node.component->focusable()) focusables_.push_back(&node);

        for (auto& child : node.children) mount_node_recorded(*child);
    }

    void rollback_failed_mount() noexcept {
        // Whole-Tree mount failure is a retained callback generation boundary.
        // Dynamic subtree failures retire only the attempted subtree identities.
        if (failed_mount_root_ && !failed_mount_root_->parent) {
            retire_retained_invalidation_generation();
        }

        for (auto it = mount_transition_progress_.rbegin();
             it != mount_transition_progress_.rend(); ++it) {
            if (!it->node) continue;
            auto& node = *it->node;

            // Retire long-lived identity before arbitrary unmount callbacks can
            // re-enter through callbacks issued by the failed mount attempt.
            if (it->live_identity_started) retire_mount_identity(node.id);

            if (it->component_started) {
                try {
                    LifecycleContext context{
                        node.id,
                        node.bounds,
                        unsafe_paint_invalidator(node),
                        unsafe_layout_invalidator(node)};
                    node.component->unmount(context);
                } catch (...) {
                    // The original mount exception owns propagation. Rollback
                    // remains best-effort through every transition that began.
                }
            }
            if (it->theme_setup_started) {
                if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
                    try {
                        binding->set_theme_change_invalidator({});
                    } catch (...) {
                    }
                }
            }
            if (it->dynamic_setup_started) {
                if (auto* source = dynamic_source(node)) {
                    try {
                        source->set_structure_invalidator({});
                    } catch (...) {
                    }
                    unregister_dynamic_node(node.id);
                }
            }
        }
        mount_transition_progress_.clear();
        failed_mount_root_ = nullptr;
    }

    void mount_node(Node& node) {
        mount_transition_progress_.clear();
        failed_mount_root_ = nullptr;
        try {
            mount_node_recorded(node);
        } catch (...) {
            // Keep the exact pre-order journal until the existing caller enters
            // unmount_node(node). The rollback wrapper below then inverts only
            // transitions whose forward hook/setup actually began.
            failed_mount_root_ = &node;
            throw;
        }
        mount_transition_progress_.clear();
    }

    void activate_node_recorded(Node& node) {
        // Record before invoking user code: if activate() itself throws, this
        // node still requires the matching best-effort deactivate().
        activation_transition_progress_.push_back(&node);
        LifecycleContext context{
            node.id,
            node.bounds,
            unsafe_paint_invalidator(node),
            unsafe_layout_invalidator(node)};
        node.component->activate(context);
        for (auto& child : node.children) activate_node_recorded(*child);
    }

    void rollback_failed_activation() noexcept {
        for (auto it = activation_transition_progress_.rbegin();
             it != activation_transition_progress_.rend(); ++it) {
            if (!*it) continue;
            auto& node = **it;
            try {
                LifecycleContext context{
                    node.id,
                    node.bounds,
                    unsafe_paint_invalidator(node),
                    unsafe_layout_invalidator(node)};
                node.component->deactivate(context);
            } catch (...) {
                // Preserve the original activate exception while completing the
                // exact reverse transition for every node that started.
            }
        }
        activation_transition_progress_.clear();
        failed_activation_root_ = nullptr;
    }

    void activate_node(Node& node) {
        activation_transition_progress_.clear();
        failed_activation_root_ = nullptr;
        try {
            activate_node_recorded(node);
        } catch (...) {
            failed_activation_root_ = &node;
            throw;
        }
        activation_transition_progress_.clear();
    }

    void deactivate_node(Node& node) {
        if (failed_activation_root_ == &node) {
            rollback_failed_activation();
            return;
        }
        deactivate_node_untracked(node);
    }

    void unmount_node(Node& node) {
        if (failed_mount_root_ == &node) {
            rollback_failed_mount();
            return;
        }

        // T129 retained callbacks become inert before arbitrary teardown code.
        // Whole-Tree unmount rotates the generation; dynamic subtree removal only
        // retires identities within that subtree. The T130 untracked helper then
        // preserves best-effort reverse traversal and first-error propagation.
        if (!node.parent) retire_retained_invalidation_generation();
        retire_subtree_identities(node);
        unmount_node_untracked(node);
    }

    std::vector<MountTransitionProgress> mount_transition_progress_;
    std::vector<Node*> activation_transition_progress_;
    Node* failed_mount_root_{};
    Node* failed_activation_root_{};
