    struct MountTransitionProgress {
        Node* node{};
        bool theme_setup_started{};
        bool dynamic_setup_started{};
        bool component_started{};
    };

    void mount_node_recorded(Node& node) {
        // Journal before the first fallible operation. A vector allocation
        // failure therefore happens before this node acquires retained state,
        // while every later failure has an exact rollback record.
        mount_transition_progress_.push_back(MountTransitionProgress{&node});
        auto& progress = mount_transition_progress_.back();

        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            progress.theme_setup_started = true;
            binding->bind_theme(node.parent ? inherited_theme_for(node) : theme_);
            binding->set_theme_change_invalidator(
                [this, id = node.id](ThemeInvalidation invalidation) {
                    apply_scoped_theme_change(id, invalidation);
                });
        }
        if (dynamic_source(node)) {
            progress.dynamic_setup_started = true;
            register_dynamic_node(node);
        }

        MountContext context{
            node.id,
            paint_invalidator(node),
            layout_invalidator(node),
            focus_invalidator(node),
            availability_invalidator(node),
            overlay_service_};
        progress.component_started = true;
        node.component->mount(context);
        if (node.component->focusable()) focusables_.push_back(&node);

        for (auto& child : node.children) mount_node_recorded(*child);
    }

    void rollback_failed_mount() noexcept {
        for (auto it = mount_transition_progress_.rbegin();
             it != mount_transition_progress_.rend(); ++it) {
            if (!it->node) continue;
            auto& node = *it->node;

            if (it->component_started) {
                try {
                    LifecycleContext context{
                        node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
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
            node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
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
                    node.id, node.bounds, paint_invalidator(node), layout_invalidator(node)};
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
        unmount_node_untracked(node);
    }

    std::vector<MountTransitionProgress> mount_transition_progress_;
    std::vector<Node*> activation_transition_progress_;
    Node* failed_mount_root_{};
    Node* failed_activation_root_{};
