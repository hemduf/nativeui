    std::vector<NodeId> dynamic_teardown_quarantine_ids_;
    std::uint64_t dynamic_teardown_quarantine_epoch_{};

    class DynamicTeardownQuarantineScope final {
    public:
        DynamicTeardownQuarantineScope(
            Tree& tree,
            Node& owner,
            const std::vector<std::string>& current_keys,
            const std::vector<std::string>& desired_keys)
            : tree_(tree) {
            const auto count = std::min(current_keys.size(), owner.children.size());
            std::vector<NodeId> quarantine;
            quarantine.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                if (std::find(desired_keys.begin(), desired_keys.end(), current_keys[i]) ==
                    desired_keys.end()) {
                    quarantine.push_back(owner.children[i]->id);
                }
            }
            if (quarantine.empty()) return;

            previous_epoch_ = tree_.dynamic_teardown_quarantine_epoch_;
            previous_.swap(tree_.dynamic_teardown_quarantine_ids_);
            tree_.dynamic_teardown_quarantine_ids_.swap(quarantine);
            tree_.dynamic_teardown_quarantine_epoch_ = tree_.structural_epoch_;
            active_ = true;
        }

        DynamicTeardownQuarantineScope(const DynamicTeardownQuarantineScope&) = delete;
        DynamicTeardownQuarantineScope& operator=(const DynamicTeardownQuarantineScope&) = delete;

        ~DynamicTeardownQuarantineScope() noexcept {
            if (!active_) return;
            tree_.dynamic_teardown_quarantine_ids_.swap(previous_);
            tree_.dynamic_teardown_quarantine_epoch_ = previous_epoch_;
        }

    private:
        Tree& tree_;
        std::vector<NodeId> previous_;
        std::uint64_t previous_epoch_{};
        bool active_{};
    };

    void reset_semantic_recovery() noexcept {
        focus_transition_stack_.clear();
        pending_focus_target_ = kInvalidNodeId;
        pending_focus_request_ = false;
        focus_blur_started_ = kInvalidNodeId;
        focus_blur_started_from_availability_ = false;
        availability_cleared_focus_ = nullptr;
        pointer_hover_transition_stack_.clear();
        pending_pointer_hover_transition_.reset();
        dynamic_teardown_quarantine_ids_.clear();
        dynamic_teardown_quarantine_epoch_ = 0;
    }
