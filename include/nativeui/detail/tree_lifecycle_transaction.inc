    // Stack-confined lifecycle/focus call sites keep the immediate invalidator
    // contract from tree_layout.inc while T129 retained callbacks use the NodeId
    // overloads/factories from tree_retained_invalidation.inc.
    [[nodiscard]] std::function<void()> paint_invalidator(Node& node) {
        return unsafe_paint_invalidator(node);
    }

    [[nodiscard]] std::function<void()> layout_invalidator(Node& node) {
        return unsafe_layout_invalidator(node);
    }

    // T130 transactions are short-lived and usually small. Keep the common
    // retained-tree journal on the caller stack so a fresh UI does not pay heap
    // allocation merely to make rollback possible. Larger/dynamic trees spill
    // into one pre-reserved vector before lifecycle publication.
    template <class T, std::size_t InlineCapacity = 64>
    class InlineTransactionJournal {
    public:
        void reserve(std::size_t total_size) {
            if (total_size > InlineCapacity) overflow_.reserve(total_size - InlineCapacity);
        }

        void clear() noexcept {
            size_ = 0;
            overflow_.clear();
        }

        void push_back(T value) {
            if (size_ < InlineCapacity) {
                inline_[size_] = std::move(value);
            } else {
                overflow_.push_back(std::move(value));
            }
            ++size_;
        }

        [[nodiscard]] T& back() noexcept { return (*this)[size_ - 1]; }
        [[nodiscard]] const T& back() const noexcept { return (*this)[size_ - 1]; }

        [[nodiscard]] T& operator[](std::size_t index) noexcept {
            return index < InlineCapacity ? inline_[index] : overflow_[index - InlineCapacity];
        }

        [[nodiscard]] const T& operator[](std::size_t index) const noexcept {
            return index < InlineCapacity ? inline_[index] : overflow_[index - InlineCapacity];
        }

        [[nodiscard]] std::size_t size() const noexcept { return size_; }

    private:
        T inline_[InlineCapacity]{};
        std::vector<T> overflow_;
        std::size_t size_{};
    };

    struct MountTransitionProgress {
        Node* node{};
        bool live_identity_started{};
        bool theme_setup_started{};
        bool dynamic_setup_started{};
        bool component_started{};
    };

    using MountTransitionJournal = InlineTransactionJournal<MountTransitionProgress>;
    using ActivationTransitionJournal = InlineTransactionJournal<Node*>;

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

    void mount_node_recorded(
        Node& node,
        MountTransitionJournal& journal,
        bool publish_focusables) {
        // Journal before the first fallible operation. The inline journal cannot
        // allocate for the common <=64-node path; overflow capacity, when needed,
        // was reserved before any lifecycle publication.
        journal.push_back(MountTransitionProgress{&node});
        auto& progress = journal.back();

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
        if (publish_focusables && node.component->focusable()) focusables_.push_back(&node);

        for (auto& child : node.children) {
            mount_node_recorded(*child, journal, publish_focusables);
        }
    }

    void rollback_failed_mount(MountTransitionJournal& journal, Node& failed_root) noexcept {
        // Whole-Tree mount failure is a retained callback generation boundary.
        // Retire it before arbitrary rollback callbacks can re-enter. Dynamic
        // subtree failures retire only the attempted subtree identities below.
        if (journal.size() != 0 && !failed_root.parent) {
            retire_retained_invalidation_generation();
        }

        for (std::size_t index = journal.size(); index-- > 0;) {
            auto& progress = journal[index];
            if (!progress.node) continue;
            auto& node = *progress.node;

            if (progress.live_identity_started) retire_mount_identity(node.id);

            if (progress.component_started) {
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
            if (progress.theme_setup_started) {
                if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
                    try {
                        binding->set_theme_change_invalidator({});
                    } catch (...) {
                    }
                }
            }
            if (progress.dynamic_setup_started) {
                if (auto* source = dynamic_source(node)) {
                    try {
                        source->set_structure_invalidator({});
                    } catch (...) {
                    }
                    try {
                        unregister_dynamic_node(node.id);
                    } catch (...) {
                    }
                }
            }
        }
        journal.clear();
    }

    void mount_node(Node& node) {
        MountTransitionJournal journal;
        failed_mount_root_ = nullptr;

        // Whole-Tree mount publishes the initial focus registry as before. A
        // non-root mount is a provisional retained insertion: its structure is
        // already staged by the dynamic transaction, but its focusability must
        // remain invisible until that transaction completes mount+activation and
        // performs the existing staged focus-registry commit.
        const bool publish_focusables = node.parent == nullptr;
        try {
            // Reserve only spill capacity. The static multi_instance fixture has
            // 47 nodes and remains entirely stack-backed; larger trees still get
            // one bounded pre-publication allocation instead of geometric growth.
            journal.reserve(retained_subtree_node_count(node));
            mount_node_recorded(node, journal, publish_focusables);
        } catch (...) {
            rollback_failed_mount(journal, node);
            // Tree::mount()/dynamic reconciliation still invoke unmount_node()
            // from their outer recovery path. The journal has already performed
            // the exact rollback while it was alive, so consume that call once.
            failed_mount_root_ = &node;
            throw;
        }
    }

    void activate_node_recorded(Node& node, ActivationTransitionJournal& journal) {
        // Record before invoking user code: if activate() itself throws, this
        // node still requires the matching best-effort deactivate().
        journal.push_back(&node);
        LifecycleContext context{
            node.id,
            node.bounds,
            unsafe_paint_invalidator(node),
            unsafe_layout_invalidator(node)};
        node.component->activate(context);
        for (auto& child : node.children) activate_node_recorded(*child, journal);
    }

    void rollback_failed_activation(ActivationTransitionJournal& journal) noexcept {
        for (std::size_t index = journal.size(); index-- > 0;) {
            auto* node = journal[index];
            if (!node) continue;
            try {
                LifecycleContext context{
                    node->id,
                    node->bounds,
                    unsafe_paint_invalidator(*node),
                    unsafe_layout_invalidator(*node)};
                node->component->deactivate(context);
            } catch (...) {
                // Preserve the original activate exception while completing the
                // exact reverse transition for every node that started.
            }
        }
        journal.clear();
    }

    void activate_node(Node& node) {
        ActivationTransitionJournal journal;
        failed_activation_root_ = nullptr;
        try {
            journal.reserve(retained_subtree_node_count(node));
            activate_node_recorded(node, journal);
        } catch (...) {
            rollback_failed_activation(journal);
            failed_activation_root_ = &node;
            throw;
        }
    }

    void deactivate_node(Node& node) {
        if (failed_activation_root_ == &node) {
            failed_activation_root_ = nullptr;
            return;
        }
        deactivate_node_untracked(node);
    }

    void unmount_node(Node& node) {
        if (failed_mount_root_ == &node) {
            failed_mount_root_ = nullptr;
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

    Node* failed_mount_root_{};
    Node* failed_activation_root_{};