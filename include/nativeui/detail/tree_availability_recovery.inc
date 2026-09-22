    void sync_availability_inactive() {
        if (!availability_structure_dirty_ || !root_) return;
        mark_paint_culling_dirty();

        try {
            std::vector<PendingAvailability> pending;
            pending.reserve(focusables_.size() + 8);
            collect_availability(*root_, ComponentAvailability{}, pending);
            availability_structure_dirty_ = false;

            bool layout_changed = false;
            bool focus_changed = false;
            bool paint_changed = false;
            apply_pending_availability(pending, layout_changed, focus_changed, paint_changed);

            if (focus_changed) focus_structure_dirty_ = true;
            if (layout_changed) {
                invalidate_layout_from(*root_);
            } else if (paint_changed) {
                publish_all_visual_bounds_then_invalidate();
            }
        } catch (...) {
            availability_structure_dirty_ = true;
            focus_structure_dirty_ = true;
            layout_dirty_ = true;
            throw;
        }
    }

    void sync_availability_structure(PlatformServices& platform) {
        if (!availability_structure_dirty_ || syncing_availability_ || !root_) return;
        mark_paint_culling_dirty();

        const bool previous_syncing = syncing_availability_;
        syncing_availability_ = true;
        Node* rehome_scope = nullptr;
        bool focus_needs_rehome = false;
        constexpr int kMaxReconcilePasses = 16;

        try {
            for (int pass = 0; pass < kMaxReconcilePasses && availability_structure_dirty_; ++pass) {
                availability_structure_dirty_ = false;

                std::vector<PendingAvailability> pending;
                pending.reserve(focusables_.size() + 8);
                collect_availability(*root_, ComponentAvailability{}, pending);

                bool any_change = false;
                for (const auto& change : pending) {
                    if (!(change.value == change.node->component->effective_availability())) {
                        any_change = true;
                        break;
                    }
                }
                if (!any_change) {
                    if (focus_needs_rehome && active_ && !focus_active_) {
                        Node* fallback = nullptr;
                        if (rehome_scope &&
                            rehome_scope->component->effective_availability().interactive()) {
                            fallback = first_available_focusable(rehome_scope);
                        } else {
                            fallback = first_available_focusable();
                        }
                        if (fallback) set_focus(fallback, platform);
                        focus_needs_rehome = false;
                        rehome_scope = nullptr;
                    }
                    continue;
                }

                if (auto* capture = pointer_capture_owner()) {
                    if (!pending_availability_for(pending, capture).interactive()) {
                        (void)cancel_pointer_capture(platform);
                    }
                }

                if (pointer_hover_target_ &&
                    !pending_availability_for(pending, pointer_hover_target_).interactive()) {
                    InputEvent leave{};
                    leave.type = InputType::PointerLeave;
                    (void)clear_pointer_hover(leave, platform);
                }

                if (auto* focused = current_focus_node()) {
                    if (!pending_availability_for(pending, focused).interactive()) {
                        rehome_scope = trapping_scope_for(focused);
                        focus_needs_rehome = true;
                        clear_focus(platform);
                        if (!availability_cleared_focus_) availability_cleared_focus_ = focused;
                    }
                }

                if (availability_structure_dirty_) continue;

                bool layout_changed = false;
                bool focus_changed = false;
                bool paint_changed = false;
                apply_pending_availability(pending, layout_changed, focus_changed, paint_changed);

                if (focus_changed) focus_structure_dirty_ = true;
                if (layout_changed) {
                    invalidate_layout_from(*root_);
                } else if (paint_changed) {
                    // The effective paint-participation state is now committed.
                    // Republish authoritative visual bounds before a later
                    // selective traversal is allowed to rebuild its cache.
                    publish_all_visual_bounds_then_invalidate();
                }

                if (focus_changed && active_) sync_focus_structure(platform);

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

                if (focus_needs_rehome && active_ && !focus_active_) {
                    Node* fallback = nullptr;
                    if (rehome_scope &&
                        rehome_scope->component->effective_availability().interactive()) {
                        fallback = first_available_focusable(rehome_scope);
                    } else {
                        fallback = first_available_focusable();
                    }
                    if (fallback) set_focus(fallback, platform);
                    focus_needs_rehome = false;
                    rehome_scope = nullptr;
                }
            }
            syncing_availability_ = previous_syncing;
        } catch (...) {
            availability_structure_dirty_ = true;
            focus_structure_dirty_ = true;
            layout_dirty_ = true;
            syncing_availability_ = previous_syncing;
            throw;
        }

        if (availability_structure_dirty_) {
            invalidate_paint(viewport_rect());
        }
    }
