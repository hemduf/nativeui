        for (auto* current = node; current; current = current->parent) {
            if (current == ancestor) return true;
        }
        return false;
    }

    void notify_focus_within_transition(
        Node* previous, Node* next, PlatformServices& platform) {
        const auto dispatcher = detail::dispatcher_for_platform(platform);

        // Only ancestors whose focus-within truth value changes are notified.
        // Moving focus between two descendants of the same decorator therefore
        // does not synthesize a false->true cycle or restart delayed policy.
        for (auto* current = previous; current; current = current->parent) {
            if (next && is_descendant_or_self(next, current)) continue;
            if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                    current->component.get())) {
                observer->retained_focus_within_changed(
                    false, pointer_interaction_active_, dispatcher);
            }
        }
        for (auto* current = next; current; current = current->parent) {
            if (previous && is_descendant_or_self(previous, current)) continue;
            if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                    current->component.get())) {
                observer->retained_focus_within_changed(
                    true, pointer_interaction_active_, dispatcher);
            }
        }
    }

    enum class FocusTransitionPhase {
        BlurOld,
        InvalidateOld,
        Commit,
        FocusNew,
        InvalidateNew,
        FocusWithinLeave,
        FocusWithinEnter,
        Descendant,
        Complete,
    };

    struct FocusTransitionState {
        NodeId previous{kInvalidNodeId};
        NodeId next{kInvalidNodeId};
        NodeId cursor{kInvalidNodeId};
        FocusTransitionPhase phase{FocusTransitionPhase::BlurOld};
    };

    std::vector<FocusTransitionState> focus_transition_stack_;
    std::size_t focus_transition_running_{};
    std::size_t focus_semantic_running_{};
    NodeId pending_focus_target_{kInvalidNodeId};
    bool pending_focus_request_{};

    [[nodiscard]] Node* focus_transition_node(NodeId id) noexcept {
        return id == kInvalidNodeId || !root_ ? nullptr : find_node_mutable(*root_, id);
    }

    void queue_pending_focus(NodeId target) noexcept {
        pending_focus_target_ = target;
        pending_focus_request_ = true;
    }

    void execute_focus_transition(std::size_t index, PlatformServices& platform) {
        ++focus_transition_running_;
        try {
            while (index < focus_transition_stack_.size() &&
                   focus_transition_stack_[index].phase != FocusTransitionPhase::Complete) {
                switch (focus_transition_stack_[index].phase) {
                case FocusTransitionPhase::BlurOld: {
                    const auto id = focus_transition_stack_[index].previous;
                    auto* old = focus_transition_node(id);
                    if (!old) {
                        focus_transition_stack_[index].phase = FocusTransitionPhase::Commit;
                        break;
                    }
                    FocusContext context{
                        old->bounds,
                        platform,
                        unsafe_paint_invalidator(*old),
                        unsafe_layout_invalidator(*old)};
                    // Preparation above may allocate. Advance only once the next
                    // operation is the application callback itself: if it throws,
                    // recovery never invokes that already-started callback again.
                    focus_transition_stack_[index].phase = FocusTransitionPhase::InvalidateOld;
                    old->component->focus_changed(false, context);
                    break;
                }
                case FocusTransitionPhase::InvalidateOld: {
                    const auto id = focus_transition_stack_[index].previous;
                    focus_transition_stack_[index].phase = FocusTransitionPhase::Commit;
                    if (auto* old = focus_transition_node(id)) invalidate_node_paint(*old);
                    break;
                }
                case FocusTransitionPhase::Commit: {
                    const auto next_id = focus_transition_stack_[index].next;
                    auto* next = focus_transition_node(next_id);
                    if (!next) {
                        focus_active_ = false;
                        focus_transition_stack_[index].phase = FocusTransitionPhase::FocusWithinLeave;
                        focus_transition_stack_[index].cursor =
                            focus_transition_stack_[index].previous;
                        break;
                    }
                    const auto it = std::find(focusables_.begin(), focusables_.end(), next);
                    if (it == focusables_.end()) {
                        focus_active_ = false;
                        focus_transition_stack_[index].phase = FocusTransitionPhase::FocusWithinLeave;
                        focus_transition_stack_[index].cursor =
                            focus_transition_stack_[index].previous;
                        break;
                    }
                    focused_index_ = static_cast<std::size_t>(
                        std::distance(focusables_.begin(), it));
                    focus_active_ = true;
                    focus_transition_stack_[index].phase = FocusTransitionPhase::FocusNew;
                    break;
                }
                case FocusTransitionPhase::FocusNew: {
                    const auto id = focus_transition_stack_[index].next;
                    auto* next = focus_transition_node(id);
                    if (!next) {
                        focus_transition_stack_[index].phase = FocusTransitionPhase::FocusWithinLeave;
                        focus_transition_stack_[index].cursor =
                            focus_transition_stack_[index].previous;
                        break;
                    }
                    FocusContext context{
                        next->bounds,
                        platform,
                        unsafe_paint_invalidator(*next),
                        unsafe_layout_invalidator(*next)};
                    focus_transition_stack_[index].phase = FocusTransitionPhase::InvalidateNew;
                    next->component->focus_changed(true, context);
                    break;
                }
                case FocusTransitionPhase::InvalidateNew: {
                    const auto id = focus_transition_stack_[index].next;
                    focus_transition_stack_[index].phase = FocusTransitionPhase::FocusWithinLeave;
                    focus_transition_stack_[index].cursor =
                        focus_transition_stack_[index].previous;
                    if (auto* next = focus_transition_node(id)) invalidate_node_paint(*next);
                    break;
                }
                case FocusTransitionPhase::FocusWithinLeave: {
                    const auto next_id = focus_transition_stack_[index].next;
                    auto* const next = focus_transition_node(next_id);
                    auto cursor = focus_transition_stack_[index].cursor;
                    while (cursor != kInvalidNodeId) {
                        auto* current = focus_transition_node(cursor);
                        if (!current) {
                            focus_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = current->parent
                            ? current->parent->id
                            : kInvalidNodeId;
                        if (next && is_descendant_or_self(next, current)) {
                            focus_transition_stack_[index].cursor = following;
                            cursor = following;
                            continue;
                        }
                        if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                                current->component.get())) {
                            const auto dispatcher = detail::dispatcher_for_platform(platform);
                            // The dispatcher is prepared before the cursor moves.
                            // Once the cursor advances, only the callback can throw.
                            focus_transition_stack_[index].cursor = following;
                            observer->retained_focus_within_changed(
                                false, pointer_interaction_active_, dispatcher);
                        } else {
                            focus_transition_stack_[index].cursor = following;
                        }
                        cursor = following;
                    }
                    if (focus_transition_stack_[index].cursor == kInvalidNodeId) {
                        focus_transition_stack_[index].phase =
                            FocusTransitionPhase::FocusWithinEnter;
                        focus_transition_stack_[index].cursor =
                            focus_transition_stack_[index].next;
                    }
                    break;
                }
                case FocusTransitionPhase::FocusWithinEnter: {
                    const auto previous_id = focus_transition_stack_[index].previous;
                    auto* const previous = focus_transition_node(previous_id);
                    auto cursor = focus_transition_stack_[index].cursor;
                    while (cursor != kInvalidNodeId) {
                        auto* current = focus_transition_node(cursor);
                        if (!current) {
                            focus_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = current->parent
                            ? current->parent->id
                            : kInvalidNodeId;
                        if (previous && is_descendant_or_self(previous, current)) {
                            focus_transition_stack_[index].cursor = following;
                            cursor = following;
                            continue;
                        }
                        if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                                current->component.get())) {
                            const auto dispatcher = detail::dispatcher_for_platform(platform);
                            focus_transition_stack_[index].cursor = following;
                            observer->retained_focus_within_changed(
                                true, pointer_interaction_active_, dispatcher);
                        } else {
                            focus_transition_stack_[index].cursor = following;
                        }
                        cursor = following;
                    }
                    if (focus_transition_stack_[index].cursor == kInvalidNodeId) {
                        focus_transition_stack_[index].phase = FocusTransitionPhase::Descendant;
                        if (auto* next = focus_transition_node(
                                focus_transition_stack_[index].next)) {
                            focus_transition_stack_[index].cursor =
                                next->parent ? next->parent->id : kInvalidNodeId;
                        }
                    }
                    break;
                }
                case FocusTransitionPhase::Descendant: {
                    const auto next_id = focus_transition_stack_[index].next;
                    auto* const next = focus_transition_node(next_id);
                    auto cursor = focus_transition_stack_[index].cursor;
                    while (next && cursor != kInvalidNodeId) {
                        auto* ancestor = focus_transition_node(cursor);
                        if (!ancestor) {
                            focus_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = ancestor->parent
                            ? ancestor->parent->id
                            : kInvalidNodeId;
                        const Rect descendant_bounds{
                            next->bounds.x - ancestor->bounds.x,
                            next->bounds.y - ancestor->bounds.y,
                            next->bounds.w,
                            next->bounds.h};
                        focus_transition_stack_[index].cursor = following;
                        ancestor->component->descendant_focus_changed(descendant_bounds);
                        cursor = following;
                    }
                    if (!next || focus_transition_stack_[index].cursor == kInvalidNodeId) {
                        focus_transition_stack_[index].phase = FocusTransitionPhase::Complete;
                        if (!syncing_availability_ || !focus_structure_dirty_) {
                            availability_cleared_focus_ = nullptr;
                        }
                    }
                    break;
                }
                case FocusTransitionPhase::Complete:
                    break;
                }
            }
            --focus_transition_running_;
        } catch (...) {
            --focus_transition_running_;
            // Transition state is already advanced past every callback that
            // entered application code. Keep the suffix durable for the next
            // safe focus checkpoint and never invoke callbacks during unwind.
            focus_structure_dirty_ = true;
            throw;
        }
    }

    void resume_focus_transitions(PlatformServices& platform) {
        if (focus_transition_running_ != 0 || focus_semantic_running_ != 0) return;
        while (!focus_transition_stack_.empty()) {
            const auto index = focus_transition_stack_.size() - 1;
            execute_focus_transition(index, platform);
            if (index < focus_transition_stack_.size() &&
                focus_transition_stack_[index].phase == FocusTransitionPhase::Complete) {
                focus_transition_stack_.pop_back();
                continue;
            }
            break;
        }
        if (focus_transition_stack_.empty() &&
            availability_blur_started_ == kInvalidNodeId) {
            drain_one_pending_focus_request(platform);
        }
    }

    [[nodiscard]] bool focus_target_available(const Node* node) const noexcept {
        if (!node || !node->component->effective_availability().interactive()) return false;
        for (auto* current = node; current; current = current->parent) {
            if (!current->component->effective_availability().interactive()) return false;
            if (current->component->is_focus_scope() &&
                !current->component->focus_scope_active()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] Node* current_focus_node() const noexcept {
        return focus_active_ && focused_index_ < focusables_.size()
            ? focusables_[focused_index_]
            : nullptr;
    }

    [[nodiscard]] Node* trapping_scope_for(const Node* node) const noexcept {
        for (auto* current = node; current; current = current->parent) {
            if (current->component->effective_availability().interactive() &&
                current->component->is_focus_scope() &&
                current->component->focus_scope_active() &&
                current->component->focus_scope_traps()) {
                return const_cast<Node*>(current);
            }
        }
        return nullptr;
    }

    [[nodiscard]] Node* first_available_focusable(const Node* within = nullptr) const noexcept {
        for (auto* candidate : focusables_) {
            if (!focus_target_available(candidate)) continue;
            if (within && !is_descendant_or_self(candidate, within)) continue;
            return candidate;
        }
        return nullptr;
    }

    [[nodiscard]] Node* default_focus_for_scope(Node& scope) const noexcept {
        const auto requested = scope.component->focus_scope_default_index();
        std::size_t index = 0;
        Node* first = nullptr;
        for (auto* candidate : focusables_) {
            if (!focus_target_available(candidate) ||
                !is_descendant_or_self(candidate, &scope)) {
                continue;
            }
            if (!first) first = candidate;
            if (index++ == requested) return candidate;
        }
        return first;
    }

    void drain_one_pending_focus_request(PlatformServices& platform) {
        if (!pending_focus_request_ || focus_transition_running_ != 0 ||
            focus_semantic_running_ != 0 || !focus_transition_stack_.empty()) {
            return;
        }

        const auto requested = pending_focus_target_;
        Node* next = nullptr;
        if (requested != kInvalidNodeId) {
            next = focus_transition_node(requested);
            if (!focus_target_available(next)) {
                pending_focus_request_ = false;
                return;
            }
            if (auto* current_trap = trapping_scope_for(current_focus_node());
                current_trap && !is_descendant_or_self(next, current_trap) &&
                trapping_scope_for(next) == nullptr) {
                pending_focus_request_ = false;
                return;
            }
            const auto it = std::find(focusables_.begin(), focusables_.end(), next);
            if (it == focusables_.end()) {
                pending_focus_request_ = false;
                return;
            }
            if (focus_active_ && focusables_[focused_index_] == next) {
                pending_focus_request_ = false;
                return;
            }
        } else if (!focus_active_ || focused_index_ >= focusables_.size()) {
            pending_focus_request_ = false;
            return;
        }

        auto* old = current_focus_node();
        FocusTransitionState transition{};
        transition.previous = old ? old->id : kInvalidNodeId;
        transition.next = next ? next->id : kInvalidNodeId;
        transition.cursor = transition.previous;
        transition.phase = old ? FocusTransitionPhase::BlurOld : FocusTransitionPhase::Commit;

        // Allocation is the last fallible operation before consuming the
        // coalesced request. If push_back throws, the pending intent remains
        // authoritative and can be retried at a later safe checkpoint.
        focus_transition_stack_.push_back(transition);
        pending_focus_request_ = false;
        if (!next) {
            availability_cleared_focus_ = syncing_availability_ ? old : nullptr;
            availability_blur_started_ = kInvalidNodeId;
        }

        const auto index = focus_transition_stack_.size() - 1;
        execute_focus_transition(index, platform);
        if (index < focus_transition_stack_.size() &&
            focus_transition_stack_[index].phase == FocusTransitionPhase::Complete) {
            focus_transition_stack_.pop_back();
        }
    }

    // Stable identity of an availability-driven focus-loss notification that
    // already entered application code but did not reach the retained commit
    // point. A later availability checkpoint may finish framework bookkeeping
    // for this exact node without retrying the already-started callback.
    NodeId availability_blur_started_{kInvalidNodeId};

    void clear_focus(PlatformServices& platform) {
        const bool resume_availability_blur =
            syncing_availability_ && focus_active_ && focused_index_ < focusables_.size() &&
            availability_blur_started_ == focusables_[focused_index_]->id;
        if (!resume_availability_blur &&
            (focus_transition_running_ != 0 || focus_semantic_running_ != 0 ||
             !focus_transition_stack_.empty() || pending_focus_request_)) {
            queue_pending_focus(kInvalidNodeId);
            if (focus_transition_running_ == 0 && focus_semantic_running_ == 0) {
                resume_focus_transitions(platform);
            }
            return;
        }

        if (!focus_active_ || focused_index_ >= focusables_.size()) {
            focus_active_ = false;
            availability_blur_started_ = kInvalidNodeId;
            if (!syncing_availability_) availability_cleared_focus_ = nullptr;
            return;
        }

        ++focus_semantic_running_;
        try {
            auto* old = focusables_[focused_index_];
            if (syncing_availability_) {
                // Preserve the exact target across adjacent availability passes. A
                // State observer may queue a reversal (disabled -> enabled) for the
                // next stable notification pass only after this reconciliation has
                // returned. Keeping the retained node identity lets that synchronous
                // follow-up pass restore focus without weakening State's stable-pass
                // semantics. Dynamic reconciliation clears this pointer before a
                // retained node is removed.
                availability_cleared_focus_ = old;
            } else {
                availability_cleared_focus_ = nullptr;
            }

            if (!resume_availability_blur) {
                FocusContext context{
                    old->bounds,
                    platform,
                    unsafe_paint_invalidator(*old),
                    unsafe_layout_invalidator(*old)};
                // Publish the stable identity only for availability reconciliation,
                // immediately before entering user code. If it throws, leave focus
                // logically active so the next pass can reconstruct the same rehome
                // decision without invoking this callback again.
                if (syncing_availability_) availability_blur_started_ = old->id;
                old->component->focus_changed(false, context);
            }

            // Preserve the established successful-path ordering. Invalidation may
            // itself throw; retaining the marker until the logical focus commit lets
            // a later availability pass finish framework work without retrying the
            // already-started focus callback.
            invalidate_node_paint(*old);

            // clear_focus has already completed the application blur and old-node
            // invalidation. Prepare the remaining semantic transition before
            // publishing inactive focus so allocation failure cannot strand a
            // committed focus-within suffix. The shared transition cursor advances
            // before every observer callback, making a throwing observer at-most-once
            // while preserving the unstarted ancestor suffix for the next safe
            // checkpoint.
            FocusTransitionState transition{};
            transition.previous = old->id;
            transition.next = kInvalidNodeId;
            transition.cursor = transition.previous;
            transition.phase = FocusTransitionPhase::Commit;
            focus_transition_stack_.push_back(transition);
            const auto index = focus_transition_stack_.size() - 1;

            // From this point the durable transition owns recovery, so the
            // availability-specific pre-commit blur marker is no longer required.
            availability_blur_started_ = kInvalidNodeId;
            execute_focus_transition(index, platform);
            if (index < focus_transition_stack_.size() &&
                focus_transition_stack_[index].phase == FocusTransitionPhase::Complete) {
                focus_transition_stack_.pop_back();
            }
            --focus_semantic_running_;
        } catch (...) {
            --focus_semantic_running_;
            focus_structure_dirty_ = true;
            throw;
        }

        if (pending_focus_request_) resume_focus_transitions(platform);
    }

    void move_focus(int direction, PlatformServices& platform) {
        if (focusables_.empty()) return;
        auto* current = current_focus_node();
        auto* trap = trapping_scope_for(current);
        const auto count = focusables_.size();
        std::size_t start = current ? focused_index_ : 0;
        for (std::size_t step = 1; step <= count; ++step) {
            const auto index = direction > 0
                ? (start + step) % count
                : (start + count - (step % count)) % count;
            auto* candidate = focusables_[index];
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;
            set_focus(candidate, platform);
            return;
        }
    }

    [[nodiscard]] static bool scope_effectively_active(const Node& scope) noexcept {
        if (!scope.component->effective_availability().interactive() ||
            !scope.component->is_focus_scope() || !scope.component->focus_scope_active()) {
            return false;
        }
        for (auto* parent = scope.parent; parent; parent = parent->parent) {
            if (!parent->component->effective_availability().interactive()) return false;
            if (parent->component->is_focus_scope() &&
                !parent->component->focus_scope_active()) {
                return false;
            }
        }
        return true;
    }

    static void reset_focus_scope_cache(Node& node) noexcept {
        node.focus_scope_active_cached = false;
        node.focus_restore = kInvalidNodeId;
        for (auto& child : node.children) reset_focus_scope_cache(*child);
    }

    void reconcile_scope_deactivations(Node& node, PlatformServices& platform) {
        for (auto& child : node.children) reconcile_scope_deactivations(*child, platform);
        if (!node.component->is_focus_scope()) return;
        const bool now = scope_effectively_active(node);
        if (node.focus_scope_active_cached && !now) {
            auto* focused = current_focus_node();
            auto* transition_focus = availability_cleared_focus_ &&
                    is_descendant_or_self(availability_cleared_focus_, &node)
                ? availability_cleared_focus_
                : focused;
            const auto restore_id = node.focus_restore;
            node.focus_scope_active_cached = false;
            node.focus_restore = kInvalidNodeId;
            if (transition_focus && is_descendant_or_self(transition_focus, &node)) {
                Node* restore = restore_id == kInvalidNodeId
                    ? nullptr
                    : find_node_mutable(*root_, restore_id);
                if (focus_target_available(restore)) {
                    set_focus(restore, platform);
                } else if (auto* fallback = first_available_focusable()) {
                    set_focus(fallback, platform);
                } else {
                    clear_focus(platform);
                }
            }
        }
    }

    void reconcile_scope_activations(Node& node, PlatformServices& platform) {
        if (node.component->is_focus_scope()) {
            const bool now = scope_effectively_active(node);
            if (!node.focus_scope_active_cached && now) {
                node.focus_scope_active_cached = true;
                if (auto* restore = current_focus_node()) {
                    node.focus_restore = restore->id;
                } else {
                    node.focus_restore = kInvalidNodeId;
                }
                if (auto* target = default_focus_for_scope(node)) {
                    set_focus(target, platform);
                }
            }
        }
        for (auto& child : node.children) reconcile_scope_activations(*child, platform);
    }

    void sync_focus_structure(PlatformServices& platform) {
        resume_focus_transitions(platform);
        if (!focus_structure_dirty_) return;

        // A failed availability-driven blur can be cancelled by a re-entrant
        // state reversal before the next checkpoint. If the same node is still
        // the authoritative, available focus target, the aborted transition no
        // longer needs completion; retire only the framework marker without
        // invoking application code.
        if (availability_blur_started_ != kInvalidNodeId) {
            auto* focused = current_focus_node();
            if (!focused || focused->id != availability_blur_started_ ||
                focus_target_available(focused)) {
                availability_blur_started_ = kInvalidNodeId;
            }
        }

        reconcile_scope_deactivations(*root_, platform);
        reconcile_scope_activations(*root_, platform);
        if (focus_active_ && !focus_target_available(current_focus_node())) {
            if (auto* fallback = first_available_focusable()) set_focus(fallback, platform);
            else clear_focus(platform);
        }
        if (!focus_active_ && focus_target_available(availability_cleared_focus_)) {
            set_focus(availability_cleared_focus_, platform);
        }
        if (focus_active_) availability_cleared_focus_ = nullptr;
        focus_structure_dirty_ = false;
    }

    void set_focus(Node* node, PlatformServices& platform) {
        if (!node || !focus_target_available(node)) return;

        // Pointer targeting and other direct focus moves must not let an
        // already-active trapping scope leak focus into an ordinary sibling.
        // A target that is itself inside another active trapping scope remains
        // eligible so a newly activated/nested modal can deterministically take
        // focus from the previous trap.
        if (auto* current_trap = trapping_scope_for(current_focus_node());
            current_trap && !is_descendant_or_self(node, current_trap) &&
            trapping_scope_for(node) == nullptr) {
            return;
        }

        const auto it = std::find(focusables_.begin(), focusables_.end(), node);
        if (it == focusables_.end()) return;

        if (focus_transition_running_ != 0 || focus_semantic_running_ != 0 ||
            !focus_transition_stack_.empty() || pending_focus_request_) {
            queue_pending_focus(node->id);
            if (focus_transition_running_ == 0 && focus_semantic_running_ == 0) {
                resume_focus_transitions(platform);
            }
            return;
        }

        const auto next_index = static_cast<std::size_t>(std::distance(focusables_.begin(), it));
        if (focus_active_ && next_index == focused_index_) return;

        auto* old = current_focus_node();
        FocusTransitionState transition{};
        transition.previous = old ? old->id : kInvalidNodeId;
        transition.next = node->id;
        transition.cursor = transition.previous;
        transition.phase = old ? FocusTransitionPhase::BlurOld : FocusTransitionPhase::Commit;

        // Prepare the durable transition before publishing focus state or
        // entering application code. Allocation failure leaves old focus intact.
        focus_transition_stack_.push_back(transition);
        const auto index = focus_transition_stack_.size() - 1;
        execute_focus_transition(index, platform);
        if (index < focus_transition_stack_.size() &&
            focus_transition_stack_[index].phase == FocusTransitionPhase::Complete) {
            focus_transition_stack_.pop_back();
        }
        if (pending_focus_request_) resume_focus_transitions(platform);
    }

    [[nodiscard]] EventResult route_from(