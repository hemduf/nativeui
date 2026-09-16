        Node& target, const InputEvent& event, PlatformServices& platform) {
        for (Node* node = &target; node; node = node->parent) {
            const auto result = send_to(*node, event, platform);
            if (handled(result)) return result;
        }
        return EventResult::Ignored;
    }

    struct PointerCaptureState {
        Node* node{};
        NodeId owner{kInvalidNodeId};
    };

    [[nodiscard]] Node* pointer_capture_owner() noexcept {
        if (!pointer_capture_.node) return nullptr;
        if (pointer_capture_.owner == kInvalidNodeId ||
            pointer_capture_.node->id != pointer_capture_.owner) {
            clear_pointer_capture();
            return nullptr;
        }
        return pointer_capture_.node;
    }

    void set_pointer_capture(Node& node) noexcept {
        if (cancelling_pointer_capture_ || dynamic_teardown_quarantines(&node)) return;
        if (pointer_capture_.node == &node && pointer_capture_.owner == node.id) return;

        if (pointer_capture_.node) clear_pointer_capture();
        pointer_capture_ = PointerCaptureState{&node, node.id};
        if (active_platform_) active_platform_->begin_pointer_capture();
    }

    void release_pointer_capture(Node& node) noexcept {
        if (pointer_capture_.node == &node && pointer_capture_.owner == node.id) {
            clear_pointer_capture();
        }
    }

    void clear_pointer_capture() noexcept {
        if (!pointer_capture_.node) return;
        pointer_capture_ = {};
        if (active_platform_) active_platform_->end_pointer_capture();
    }

    enum class PointerHoverTransitionPhase {
        ObserverLeave,
        ObserverEnter,
        PointerLeave,
        Complete,
    };

    struct PointerHoverTransitionState {
        NodeId previous{kInvalidNodeId};
        NodeId next{kInvalidNodeId};
        NodeId cursor{kInvalidNodeId};
        InputEvent leave{};
        PointerHoverTransitionPhase phase{PointerHoverTransitionPhase::ObserverLeave};
    };

    std::vector<PointerHoverTransitionState> pointer_hover_transition_stack_;
    std::size_t pointer_hover_transition_running_{};
    std::unique_ptr<PointerHoverTransitionState> pending_pointer_hover_transition_;
    bool fail_next_input_context_preparation_for_testing_{};

    [[nodiscard]] Node* pointer_hover_transition_node(NodeId id) noexcept {
        return id == kInvalidNodeId || !root_ ? nullptr : find_node_mutable(*root_, id);
    }

    [[nodiscard]] InputContext prepare_input_context(
        Node& node, PlatformServices& platform) {
        if (fail_next_input_context_preparation_for_testing_) {
            fail_next_input_context_preparation_for_testing_ = false;
            throw std::bad_alloc{};
        }
        return InputContext{
            node.bounds,
            platform,
            unsafe_paint_invalidator(node),
            unsafe_layout_invalidator(node),
            [this, &node] { set_pointer_capture(node); },
            [this, &node] { release_pointer_capture(node); }};
    }

    [[nodiscard]] EventResult send_prepared_to(
        Node& node, const InputEvent& event, InputContext& context) {
        try {
            return node.component->input(event, context);
        } catch (...) {
            if (event.type == InputType::PointerDown ||
                event.type == InputType::PointerUp ||
                event.type == InputType::PointerCancel) {
                // Component input may acquire or retain capture before throwing.
                // Framework-only cleanup is noexcept and performs no component
                // callback, so later pointer input starts from a coherent idle
                // interaction instead of inheriting a failed native capture.
                clear_pointer_capture();
                pointer_interaction_active_ = false;
            }
            throw;
        }
    }

    void queue_pending_pointer_hover_transition(
        Node* next, const InputEvent& source_event) {
        // Build the complete replacement before publishing it. InputEvent owns
        // strings/vectors and its copy can allocate; allocation/copy failure
        // therefore preserves any older pending request exactly.
        auto pending = std::make_unique<PointerHoverTransitionState>();
        pending->next = next ? next->id : kInvalidNodeId;
        pending->leave = source_event;
        pending->leave.type = InputType::PointerLeave;
        pending->leave.delta = {};
        pending_pointer_hover_transition_.swap(pending);
    }

    void execute_pointer_hover_transition(std::size_t index, PlatformServices& platform) {
        ++pointer_hover_transition_running_;
        try {
            while (index < pointer_hover_transition_stack_.size() &&
                   pointer_hover_transition_stack_[index].phase !=
                       PointerHoverTransitionPhase::Complete) {
                switch (pointer_hover_transition_stack_[index].phase) {
                case PointerHoverTransitionPhase::ObserverLeave: {
                    const auto next_id = pointer_hover_transition_stack_[index].next;
                    auto* const next = pointer_hover_transition_node(next_id);
                    auto cursor = pointer_hover_transition_stack_[index].cursor;
                    while (cursor != kInvalidNodeId) {
                        auto* current = pointer_hover_transition_node(cursor);
                        if (!current) {
                            pointer_hover_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = current->parent
                            ? current->parent->id
                            : kInvalidNodeId;
                        if (next && is_descendant_or_self(next, current)) {
                            pointer_hover_transition_stack_[index].cursor = following;
                            cursor = following;
                            continue;
                        }
                        if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                                current->component.get())) {
                            const auto dispatcher = detail::dispatcher_for_platform(platform);
                            pointer_hover_transition_stack_[index].cursor = following;
                            observer->retained_pointer_hover_changed(
                                false, pointer_interaction_active_, dispatcher);
                        } else {
                            pointer_hover_transition_stack_[index].cursor = following;
                        }
                        cursor = following;
                    }
                    if (pointer_hover_transition_stack_[index].cursor == kInvalidNodeId) {
                        pointer_hover_transition_stack_[index].phase =
                            PointerHoverTransitionPhase::ObserverEnter;
                        pointer_hover_transition_stack_[index].cursor =
                            pointer_hover_transition_stack_[index].next;
                    }
                    break;
                }
                case PointerHoverTransitionPhase::ObserverEnter: {
                    const auto previous_id = pointer_hover_transition_stack_[index].previous;
                    auto* const previous = pointer_hover_transition_node(previous_id);
                    auto cursor = pointer_hover_transition_stack_[index].cursor;
                    while (cursor != kInvalidNodeId) {
                        auto* current = pointer_hover_transition_node(cursor);
                        if (!current) {
                            pointer_hover_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = current->parent
                            ? current->parent->id
                            : kInvalidNodeId;
                        if (previous && is_descendant_or_self(previous, current)) {
                            pointer_hover_transition_stack_[index].cursor = following;
                            cursor = following;
                            continue;
                        }
                        if (auto* observer = dynamic_cast<detail::RetainedInteractionObserver*>(
                                current->component.get())) {
                            const auto dispatcher = detail::dispatcher_for_platform(platform);
                            pointer_hover_transition_stack_[index].cursor = following;
                            observer->retained_pointer_hover_changed(
                                true, pointer_interaction_active_, dispatcher);
                        } else {
                            pointer_hover_transition_stack_[index].cursor = following;
                        }
                        cursor = following;
                    }
                    if (pointer_hover_transition_stack_[index].cursor == kInvalidNodeId) {
                        pointer_hover_transition_stack_[index].phase =
                            PointerHoverTransitionPhase::PointerLeave;
                        pointer_hover_transition_stack_[index].cursor =
                            pointer_hover_transition_stack_[index].previous;
                    }
                    break;
                }
                case PointerHoverTransitionPhase::PointerLeave: {
                    auto cursor = pointer_hover_transition_stack_[index].cursor;
                    while (cursor != kInvalidNodeId) {
                        auto* current = pointer_hover_transition_node(cursor);
                        if (!current) {
                            pointer_hover_transition_stack_[index].cursor = kInvalidNodeId;
                            break;
                        }
                        const auto following = current->parent
                            ? current->parent->id
                            : kInvalidNodeId;
                        // Prepare every fallible input-boundary object before
                        // committing cursor progress. A preparation failure means
                        // application input never began and this exact node stays
                        // retryable; after cursor advance only Component::input is
                        // allowed to throw, so begun callbacks remain at-most-once.
                        const auto leave = pointer_hover_transition_stack_[index].leave;
                        auto context = prepare_input_context(*current, platform);
                        pointer_hover_transition_stack_[index].cursor = following;
                        (void)send_prepared_to(*current, leave, context);
                        cursor = following;
                    }
                    if (pointer_hover_transition_stack_[index].cursor == kInvalidNodeId) {
                        pointer_hover_transition_stack_[index].phase =
                            PointerHoverTransitionPhase::Complete;
                    }
                    break;
                }
                case PointerHoverTransitionPhase::Complete:
                    break;
                }
            }
            --pointer_hover_transition_running_;
        } catch (...) {
            --pointer_hover_transition_running_;
            // Every durable cursor advances only after callback preparation and
            // immediately before application code. The unfinished suffix is
            // resumed at a later safe checkpoint without replaying started work.
            throw;
        }
    }

    void drain_one_pending_pointer_hover_transition(PlatformServices& platform) {
        if (!pending_pointer_hover_transition_ || pointer_hover_transition_running_ != 0 ||
            !pointer_hover_transition_stack_.empty()) {
            return;
        }

        const auto next_id = pending_pointer_hover_transition_->next;
        auto* next = pointer_hover_transition_node(next_id);
        if (next_id != kInvalidNodeId && !next) {
            // A structural callback may have removed the requested target before
            // the safe checkpoint. The latest semantic route is then no target.
            next = nullptr;
        }
        if (dynamic_teardown_quarantines(next)) {
            // A pending request may predate the teardown barrier. It is safe to
            // retire as no target while the node still exists, before detach.
            next = nullptr;
        }
        auto* const previous = pointer_hover_target_;
        if (previous == next) {
            pending_pointer_hover_transition_.reset();
            return;
        }

        PointerHoverTransitionState transition{};
        transition.previous = previous ? previous->id : kInvalidNodeId;
        transition.next = next ? next->id : kInvalidNodeId;
        // Copy before consuming the pending owner. If payload preparation throws,
        // the request remains intact. The vector push may allocate as well and
        // likewise happens before the pending semantic commit.
        transition.leave = pending_pointer_hover_transition_->leave;
        transition.phase = previous
            ? PointerHoverTransitionPhase::ObserverLeave
            : PointerHoverTransitionPhase::ObserverEnter;
        transition.cursor = previous ? transition.previous : transition.next;
        pointer_hover_transition_stack_.push_back(std::move(transition));
        pending_pointer_hover_transition_.reset();

        const auto index = pointer_hover_transition_stack_.size() - 1;
        pointer_hover_target_ = next;
        execute_pointer_hover_transition(index, platform);
        if (index < pointer_hover_transition_stack_.size() &&
            pointer_hover_transition_stack_[index].phase ==
                PointerHoverTransitionPhase::Complete) {
            pointer_hover_transition_stack_.pop_back();
        }
    }

    void resume_pointer_hover_transitions(PlatformServices& platform) {
        if (pointer_hover_transition_running_ != 0) return;
        while (!pointer_hover_transition_stack_.empty()) {
            const auto index = pointer_hover_transition_stack_.size() - 1;
            execute_pointer_hover_transition(index, platform);
            if (index < pointer_hover_transition_stack_.size() &&
                pointer_hover_transition_stack_[index].phase ==
                    PointerHoverTransitionPhase::Complete) {
                pointer_hover_transition_stack_.pop_back();
                continue;
            }
            break;
        }
        if (pointer_hover_transition_stack_.empty()) {
            // Drain at most one coalesced request per safe checkpoint. A callback
            // from this follow-up may replace the pending target again, but it
            // cannot recursively publish another hover transition on this stack.
            drain_one_pending_pointer_hover_transition(platform);
        }
    }

    void begin_pointer_hover_transition(
        Node* previous,
        Node* next,
        const InputEvent& source_event,
        PlatformServices& platform) {
        PointerHoverTransitionState transition{};
        transition.previous = previous ? previous->id : kInvalidNodeId;
        transition.next = next ? next->id : kInvalidNodeId;
        transition.leave = source_event;
        transition.leave.type = InputType::PointerLeave;
        transition.leave.delta = {};
        transition.phase = previous
            ? PointerHoverTransitionPhase::ObserverLeave
            : PointerHoverTransitionPhase::ObserverEnter;
        transition.cursor = previous ? transition.previous : transition.next;

        // Prepare the whole durable record before publishing the new route.
        // Allocation/copy failure therefore leaves the previous target intact.
        pointer_hover_transition_stack_.push_back(std::move(transition));
        const auto index = pointer_hover_transition_stack_.size() - 1;
        pointer_hover_target_ = next;
        execute_pointer_hover_transition(index, platform);
        if (index < pointer_hover_transition_stack_.size() &&
            pointer_hover_transition_stack_[index].phase ==
                PointerHoverTransitionPhase::Complete) {
            pointer_hover_transition_stack_.pop_back();
        }
    }

    [[nodiscard]] bool update_pointer_hover_target(
        Node* target, const InputEvent& source_event, PlatformServices& platform) {
        if (dynamic_teardown_quarantines(target)) target = nullptr;
        if (pointer_hover_transition_running_ != 0 ||
            !pointer_hover_transition_stack_.empty() || pending_pointer_hover_transition_) {
            queue_pending_pointer_hover_transition(target, source_event);
            if (pointer_hover_transition_running_ == 0) {
                resume_pointer_hover_transitions(platform);
            }
            return true;
        }

        auto* const previous = pointer_hover_target_;
        if (previous == target) return false;
        begin_pointer_hover_transition(previous, target, source_event, platform);
        if (pending_pointer_hover_transition_) resume_pointer_hover_transitions(platform);
        return true;
    }

    [[nodiscard]] bool clear_pointer_hover(
        const InputEvent& source_event, PlatformServices& platform) {
        if (pointer_hover_transition_running_ != 0 ||
            !pointer_hover_transition_stack_.empty() || pending_pointer_hover_transition_) {
            queue_pending_pointer_hover_transition(nullptr, source_event);
            if (pointer_hover_transition_running_ == 0) {
                resume_pointer_hover_transitions(platform);
            }
            return true;
        }

        auto* const previous = pointer_hover_target_;
        if (!previous) return false;
        begin_pointer_hover_transition(previous, nullptr, source_event, platform);
        if (pending_pointer_hover_transition_) resume_pointer_hover_transitions(platform);
        return true;
    }

    [[nodiscard]] EventResult cancel_pointer_capture(
        PlatformServices& platform, InputEvent event = InputEvent{}) {
        auto* owner = pointer_capture_owner();
        if (!owner) return EventResult::Ignored;

        event.type = InputType::PointerCancel;
        // Keep the owner recorded while routing so release_pointer() remains
        // idempotent, but prevent a cancellation handler from re-capturing.
        // Restore the exact prior guard value and terminate the stale capture
        // even when application code throws. Cleanup performs no component
        // callback, so the original exception can propagate to the C++ caller
        // while the later input route remains usable.
        const bool previous_cancelling = cancelling_pointer_capture_;
        cancelling_pointer_capture_ = true;
        try {
            const auto result = route_from(*owner, event, platform);
            cancelling_pointer_capture_ = previous_cancelling;
            clear_pointer_capture();
            return result;
        } catch (...) {
            cancelling_pointer_capture_ = previous_cancelling;
            clear_pointer_capture();
            throw;
        }
    }

    [[nodiscard]] EventResult send_to(
        Node& node, const InputEvent& event, PlatformServices& platform) {
        auto context = prepare_input_context(node, platform);
        return send_prepared_to(node, event, context);
    }

    std::unique_ptr<Node> root_;
    std::vector<Node*> focusables_;
    std::size_t focused_index_{};
    PointerCaptureState pointer_capture_{};
    Node* pointer_hover_target_{};
    detail::OverlayService* overlay_service_{};
    bool pointer_interaction_active_{};
    bool cancelling_pointer_capture_{};
    bool pointer_passthrough_dynamic_root_{};
    bool mounted_{};
    bool active_{};
    bool focus_active_{};
    bool focus_structure_dirty_{true};
    bool availability_structure_dirty_{};
    bool syncing_availability_{};
    Node* availability_cleared_focus_{};
    std::size_t dispatch_depth_{};
    PlatformServices* active_platform_{};
    Size viewport_{720.0f, 520.0f};
    bool layout_dirty_{true};
    DirtyRegion paint_dirty_;
    std::function<void(Rect)> invalidation_callback_;
    std::function<EventResult(Command)> global_command_handler_;