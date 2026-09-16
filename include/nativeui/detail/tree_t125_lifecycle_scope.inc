    class LifecycleTransitionScope {
    public:
        LifecycleTransitionScope(
            Tree& tree,
            LifecycleTransition requested,
            bool require_idle)
            : tree_(tree),
              requested_(requested),
              inner_(prepare(tree, requested), requested, require_idle) {}

        LifecycleTransitionScope(const LifecycleTransitionScope&) = delete;
        LifecycleTransitionScope& operator=(const LifecycleTransitionScope&) = delete;

        ~LifecycleTransitionScope() noexcept {
            if (requested_ == LifecycleTransition::Deactivate ||
                requested_ == LifecycleTransition::Unmount) {
                tree_.clear_t125_semantic_recovery_noexcept();
            }
        }

        void release() noexcept { inner_.release(); }

    private:
        static Tree& prepare(Tree& tree, LifecycleTransition requested) {
            if (requested == LifecycleTransition::Mount ||
                requested == LifecycleTransition::Activate) {
                tree.prepare_t125_semantic_recovery();
            }
            return tree;
        }

        Tree& tree_;
        LifecycleTransition requested_{LifecycleTransition::None};
        T130LifecycleTransitionScope inner_;
    };
