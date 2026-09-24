    // Stack-confined lifecycle/focus call sites keep the immediate invalidator
    // contract from tree_layout.inc while retained callbacks use the NodeId
    // overloads/factories from tree_retained_invalidation.inc.


    // Lifecycle callbacks are arbitrary application code. Keep one per-Tree
    // transition token so a callback cannot start an incompatible lifecycle
    // transaction against state that is still provisional or rolling back.
    enum class LifecycleTransition {
        None,
        Mount,
        Activate,
        Deactivate,
        Unmount,
        Paint,
    };

    class LifecycleTransitionScope {
    public:
        LifecycleTransitionScope(
            Tree& tree,
            LifecycleTransition requested,
            bool require_idle)
            : tree_(tree), previous_(tree.lifecycle_transition_) {
            if (require_idle && previous_ != LifecycleTransition::None) {
                throw std::logic_error("nested Tree lifecycle transition");
            }
            if (previous_ == LifecycleTransition::None) {
                tree_.lifecycle_transition_ = requested;
                owns_ = true;
            }
        }

        LifecycleTransitionScope(const LifecycleTransitionScope&) = delete;
        LifecycleTransitionScope& operator=(const LifecycleTransitionScope&) = delete;

        ~LifecycleTransitionScope() noexcept {
            if (owns_) tree_.lifecycle_transition_ = previous_;
        }

        void release() noexcept {
            if (!owns_) return;
            tree_.lifecycle_transition_ = previous_;
            owns_ = false;
        }

    private:
        Tree& tree_;
        LifecycleTransition previous_{LifecycleTransition::None};
        bool owns_{};
    };

    class PaintDamageTransaction final {
    public:
        PaintDamageTransaction() = default;
        PaintDamageTransaction(const PaintDamageTransaction&) = delete;
        PaintDamageTransaction& operator=(const PaintDamageTransaction&) = delete;

        PaintDamageTransaction(PaintDamageTransaction&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)),
              count_(other.count_),
              bounds_(other.bounds_),
              bounds_valid_(other.bounds_valid_) {
            for (std::size_t index = 0; index < count_; ++index) {
                captured_[index] = other.captured_[index];
            }
            other.count_ = 0;
        }

        PaintDamageTransaction& operator=(PaintDamageTransaction&& other) noexcept {
            if (this != &other) {
                rollback();
                owner_ = std::exchange(other.owner_, nullptr);
                count_ = other.count_;
                bounds_ = other.bounds_;
                bounds_valid_ = other.bounds_valid_;
                for (std::size_t index = 0; index < count_; ++index) {
                    captured_[index] = other.captured_[index];
                }
                other.count_ = 0;
            }
            return *this;
        }

        ~PaintDamageTransaction() noexcept { rollback(); }

        [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }
        [[nodiscard]] bool has_damage() const noexcept { return count_ != 0; }
        [[nodiscard]] Rect damage_bounds() const noexcept { return bounds_; }
        [[nodiscard]] bool damage_bounds_valid() const noexcept {
            return bounds_valid_;
        }

        void commit() noexcept {
            if (!owner_) return;
            owner_->scene_frame_transaction_active_ = false;
            owner_ = nullptr;
            count_ = 0;
        }

    private:
        friend class Tree;
        friend class UI;

        explicit PaintDamageTransaction(Tree& owner) noexcept {
            if (owner.scene_frame_transaction_active_ ||
                owner.lifecycle_transition_ != LifecycleTransition::None) {
                return;
            }

            owner_ = &owner;
            const auto& dirty = owner.paint_dirty_.rects();
            count_ = dirty.size();
            for (std::size_t index = 0; index < count_; ++index) {
                captured_[index] = dirty[index];
            }
            bounds_valid_ = calculate_bounds();
            owner.paint_dirty_.clear();
            owner.scene_frame_transaction_active_ = true;
        }

        [[nodiscard]] bool calculate_bounds() noexcept {
            if (count_ == 0) return true;

            double left = std::numeric_limits<double>::infinity();
            double top = std::numeric_limits<double>::infinity();
            double right = -std::numeric_limits<double>::infinity();
            double bottom = -std::numeric_limits<double>::infinity();
            for (std::size_t index = 0; index < count_; ++index) {
                const Rect rect = captured_[index];
                if (!std::isfinite(rect.x) || !std::isfinite(rect.y) ||
                    !std::isfinite(rect.w) || !std::isfinite(rect.h) ||
                    !(rect.w > 0.0f) || !(rect.h > 0.0f)) {
                    return false;
                }
                const double rect_right = static_cast<double>(rect.x) + rect.w;
                const double rect_bottom = static_cast<double>(rect.y) + rect.h;
                if (!std::isfinite(rect_right) || !std::isfinite(rect_bottom)) {
                    return false;
                }
                left = (std::min)(left, static_cast<double>(rect.x));
                top = (std::min)(top, static_cast<double>(rect.y));
                right = (std::max)(right, rect_right);
                bottom = (std::max)(bottom, rect_bottom);
            }

            constexpr double max_float =
                static_cast<double>(std::numeric_limits<float>::max());
            if (left < -max_float || top < -max_float ||
                right > max_float || bottom > max_float ||
                right - left > max_float || bottom - top > max_float) {
                return false;
            }

            const auto lower = [](double value) noexcept {
                float result = static_cast<float>(value);
                if (static_cast<double>(result) > value) {
                    result = std::nextafter(result,
                                            -std::numeric_limits<float>::infinity());
                }
                return result;
            };
            const auto upper = [](double value) noexcept {
                float result = static_cast<float>(value);
                if (static_cast<double>(result) < value) {
                    result = std::nextafter(result,
                                            std::numeric_limits<float>::infinity());
                }
                return result;
            };

            const float x = lower(left);
            const float y = lower(top);
            const float right_edge = upper(right);
            const float bottom_edge = upper(bottom);
            const double width_extent = static_cast<double>(right_edge) - x;
            const double height_extent = static_cast<double>(bottom_edge) - y;
            if (width_extent > max_float || height_extent > max_float) return false;
            const float width = upper(width_extent);
            const float height = upper(height_extent);
            if (!std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(width) || !std::isfinite(height) ||
                !(width > 0.0f) || !(height > 0.0f)) {
                return false;
            }
            bounds_ = Rect{x, y, width, height};
            return true;
        }

        void rollback() noexcept {
            if (!owner_) return;
            for (std::size_t index = 0; index < count_; ++index) {
                (void)owner_->retain_dirty(captured_[index]);
            }
            owner_->scene_frame_transaction_active_ = false;
            owner_ = nullptr;
            count_ = 0;
        }

        Tree* owner_{};
        std::array<Rect, DirtyRegion::kMaxRects> captured_{};
        std::size_t count_{};
        Rect bounds_{};
        bool bounds_valid_{true};
    };

    [[nodiscard]] PaintDamageTransaction begin_paint_damage_transaction() {
        return PaintDamageTransaction{*this};
    }

    [[nodiscard]] bool lifecycle_transition_active() const noexcept {
        return lifecycle_transition_ != LifecycleTransition::None ||
               scene_frame_transaction_active_;
    }

    // retained transactions are short-lived and usually small. Keep the common
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
        std::vector<Node*>* staged_focusables) {
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
        if (staged_focusables && node.component->focusable()) {
            staged_focusables->push_back(&node);
        }

        for (auto& child : node.children) {
            mount_node_recorded(*child, journal, staged_focusables);
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
                        paint_invalidator(node),
                        layout_invalidator(node)};
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

    void mount_node(Node& node, std::vector<Node*>* staged_focusables = nullptr) {
        LifecycleTransitionScope lifecycle{
            *this, LifecycleTransition::Mount, false};
        MountTransitionJournal journal;
        failed_mount_root_ = nullptr;
        try {
            // Prepare all storage that the mount walk itself needs before the
            // first component callback. Root focus collection piggybacks this
            // traversal; dynamic insertion passes no staging vector and keeps its
            // existing post-lifecycle focus publication transaction.
            const auto node_count = retained_subtree_node_count(node);
            journal.reserve(node_count);
            if (staged_focusables) staged_focusables->reserve(node_count);
            mount_node_recorded(node, journal, staged_focusables);
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
            paint_invalidator(node),
            layout_invalidator(node)};
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
                    paint_invalidator(*node),
                    layout_invalidator(*node)};
                node->component->deactivate(context);
            } catch (...) {
                // Preserve the original activate exception while completing the
                // exact reverse transition for every node that started.
            }
        }
        journal.clear();
    }

    void activate_node(Node& node) {
        LifecycleTransitionScope lifecycle{
            *this, LifecycleTransition::Activate, false};
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
        LifecycleTransitionScope lifecycle{
            *this, LifecycleTransition::Deactivate, false};
        if (failed_activation_root_ == &node) {
            failed_activation_root_ = nullptr;
            return;
        }
        deactivate_node_untracked(node);
    }

    void unmount_node(Node& node) {
        LifecycleTransitionScope lifecycle{
            *this, LifecycleTransition::Unmount, false};
        if (failed_mount_root_ == &node) {
            failed_mount_root_ = nullptr;
            return;
        }

        // retained callbacks become inert before arbitrary teardown code.
        // Whole-Tree unmount rotates the generation; dynamic subtree removal only
        // retires identities within that subtree. The retained untracked helper then
        // preserves best-effort reverse traversal and first-error propagation.
        if (!node.parent) retire_retained_invalidation_generation();
        retire_subtree_identities(node);
        unmount_node_untracked(node);
    }

    LifecycleTransition lifecycle_transition_{LifecycleTransition::None};
    bool scene_frame_transaction_active_{};
    Node* failed_mount_root_{};
    Node* failed_activation_root_{};
