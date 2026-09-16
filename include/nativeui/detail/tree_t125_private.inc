    void clear_t125_semantic_recovery_noexcept() noexcept {
        focus_transition_stack_.clear();
        focus_transition_running_ = 0;
        focus_semantic_running_ = 0;
        pending_focus_target_ = kInvalidNodeId;
        pending_focus_request_ = false;
        focus_blur_started_ = kInvalidNodeId;
        focus_blur_started_from_availability_ = false;
        availability_cleared_focus_ = nullptr;
        pointer_hover_transition_stack_.clear();
        pointer_hover_transition_running_ = 0;
        pending_pointer_hover_transition_.reset();
    }

    void prepare_t125_semantic_recovery() {
        if (focus_transition_stack_.capacity() < 4) focus_transition_stack_.reserve(4);
        if (pointer_hover_transition_stack_.capacity() < 4) {
            pointer_hover_transition_stack_.reserve(4);
        }
        clear_t125_semantic_recovery_noexcept();
    }
