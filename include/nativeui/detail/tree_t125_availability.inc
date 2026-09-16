    void sync_availability_inactive() {
        try {
            t130_sync_availability_inactive();
        } catch (...) {
            availability_structure_dirty_ = true;
            focus_structure_dirty_ = true;
            layout_dirty_ = true;
            throw;
        }
    }

    void sync_availability_structure(PlatformServices& platform) {
        if (!availability_structure_dirty_ || syncing_availability_ || !root_) return;

        const bool previous_syncing = syncing_availability_;
        try {
            t130_sync_availability_structure(platform);
            syncing_availability_ = previous_syncing;

            // T125 clear_focus() retains the old focus identity while availability
            // reconciliation is active. If a callback threw after blur commit, a
            // later safe checkpoint can deterministically rehome from current state.
            if (active_ && !focus_active_ && availability_cleared_focus_) {
                Node* fallback = nullptr;
                auto* recovery_scope = trapping_scope_for(availability_cleared_focus_);
                if (recovery_scope &&
                    recovery_scope->component->effective_availability().interactive()) {
                    fallback = first_available_focusable(recovery_scope);
                } else {
                    fallback = first_available_focusable();
                }
                if (fallback) set_focus(fallback, platform);
            }
        } catch (...) {
            availability_structure_dirty_ = true;
            focus_structure_dirty_ = true;
            layout_dirty_ = true;
            syncing_availability_ = previous_syncing;
            throw;
        }
    }
