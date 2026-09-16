    void reset_t125_semantic_recovery() noexcept {
        focus_transition_stack_.clear();
        pending_focus_target_ = kInvalidNodeId;
        pending_focus_request_ = false;
        focus_blur_started_ = kInvalidNodeId;
        focus_blur_started_from_availability_ = false;
        availability_cleared_focus_ = nullptr;
        pointer_hover_transition_stack_.clear();
        pending_pointer_hover_transition_.reset();
    }
