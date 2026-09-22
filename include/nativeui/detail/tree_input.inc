        Node& target, const InputEvent& event, PlatformServices& platform) {
        for (Node* node = &target; node; node = node->parent) {
            const auto result = send_to(*node, event, platform);
            if (handled(result)) return result;
        }
        return EventResult::Ignored;
    }

    static constexpr std::size_t kMaxTrackedPointers = 16U;

    struct PointerCaptureState {
        PointerId pointer{};
        Node* node{};
        NodeId owner{kInvalidNodeId};
        std::uint64_t dispatch_token{};
        bool cancelling{};
    };

    struct PointerInteractionState {
        PointerId pointer{};
        std::uint64_t dispatch_token{};
    };

    [[nodiscard]] std::size_t pointer_capture_index(PointerId pointer) const noexcept {
        for (std::size_t i = 0; i < pointer_capture_count_; ++i) {
            if (pointer_captures_[i].pointer == pointer) return i;
        }
        return pointer_capture_count_;
    }

    [[nodiscard]] Node* pointer_capture_owner(PointerId pointer) noexcept {
        const auto index = pointer_capture_index(pointer);
        if (index == pointer_capture_count_) return nullptr;
        auto& capture = pointer_captures_[index];
        if (!capture.node || capture.owner == kInvalidNodeId ||
            capture.node->id != capture.owner) {
            clear_pointer_capture(pointer);
            return nullptr;
        }
        return capture.node;
    }

    [[nodiscard]] std::uint64_t pointer_capture_dispatch_token(
        PointerId pointer) const noexcept {
        const auto index = pointer_capture_index(pointer);
        return index == pointer_capture_count_
            ? 0U
            : pointer_captures_[index].dispatch_token;
    }

    [[nodiscard]] std::optional<PointerId> first_pointer_capture_id() noexcept {
        std::size_t i = 0U;
        while (i < pointer_capture_count_) {
            const auto pointer = pointer_captures_[i].pointer;
            if (!pointer_capture_owner(pointer)) continue;

            const auto index = pointer_capture_index(pointer);
            if (index != pointer_capture_count_ &&
                (pointer == 0U || !pointer_captures_[index].cancelling)) {
                return pointer;
            }
            ++i;
        }
        return std::nullopt;
    }

    template <class Predicate>
    [[nodiscard]] std::optional<PointerId> find_pointer_capture_if(Predicate&& predicate) {
        std::size_t i = 0U;
        while (i < pointer_capture_count_) {
            const auto pointer = pointer_captures_[i].pointer;
            if (auto* owner = pointer_capture_owner(pointer)) {
                if (predicate(owner)) return pointer;
                ++i;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool node_has_pointer_capture(NodeId owner) const noexcept {
        for (std::size_t i = 0; i < pointer_capture_count_; ++i) {
            const auto& capture = pointer_captures_[i];
            if (capture.owner == owner && capture.node && capture.node->id == owner) return true;
        }
        return false;
    }

    void set_pointer_capture(
        Node& node,
        PointerId pointer,
        std::uint64_t interaction_generation) noexcept {
        // InputContext can outlive a nested dispatch on the same callback stack.
        // Never let an older frame overwrite capture owned by a newer contact
        // generation that reused the same pointer ID.
        if (pointer_interaction_dispatch_token(pointer) != interaction_generation ||
            pointer_capture_barrier_ || dynamic_teardown_quarantines(&node)) {
            return;
        }
        const auto index = pointer_capture_index(pointer);
        if (index != pointer_capture_count_) {
            auto& capture = pointer_captures_[index];
            if (!capture.cancelling &&
                capture.node == &node &&
                capture.owner == node.id) {
                capture.dispatch_token = active_input_dispatch_token_;
                return;
            }
            clear_pointer_capture(pointer);
        }
        if (pointer_capture_count_ >= pointer_captures_.size()) return;
        pointer_captures_[pointer_capture_count_++] = {
            pointer, &node, node.id, active_input_dispatch_token_};
        if (pointer == 0U && active_platform_) active_platform_->begin_pointer_capture();
    }

    void release_pointer_capture(
        Node& node,
        PointerId pointer,
        std::uint64_t interaction_generation) noexcept {
        // Symmetric with set_pointer_capture(): a stale outer callback must not
        // release a newer capture created reentrantly with the same pointer ID.
        if (pointer_interaction_dispatch_token(pointer) != interaction_generation) {
            return;
        }
        const auto index = pointer_capture_index(pointer);
        if (index != pointer_capture_count_ &&
            pointer_captures_[index].node == &node &&
            pointer_captures_[index].owner == node.id) {
            clear_pointer_capture(pointer);
        }
    }

    void clear_pointer_capture(PointerId pointer) noexcept {
        const auto index = pointer_capture_index(pointer);
        if (index == pointer_capture_count_) return;
        const bool release_platform = pointer == 0U;
        --pointer_capture_count_;
        if (index != pointer_capture_count_) {
            pointer_captures_[index] = pointer_captures_[pointer_capture_count_];
        }
        pointer_captures_[pointer_capture_count_] = {};
        if (release_platform && active_platform_) active_platform_->end_pointer_capture();
    }

    void clear_pointer_capture_if_token(
        PointerId pointer, std::uint64_t dispatch_token) noexcept {
        const auto index = pointer_capture_index(pointer);
        if (index == pointer_capture_count_ ||
            pointer_captures_[index].dispatch_token != dispatch_token) {
            return;
        }
        clear_pointer_capture(pointer);
    }

    void clear_pointer_capture() noexcept {
        bool release_platform = false;
        for (std::size_t i = 0; i < pointer_capture_count_; ++i) {
            release_platform = release_platform || pointer_captures_[i].pointer == 0U;
            pointer_captures_[i] = {};
        }
        pointer_capture_count_ = 0U;
        if (release_platform && active_platform_) active_platform_->end_pointer_capture();
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
        Node& node, const InputEvent& event, PlatformServices& platform) {
        if (fail_next_input_context_preparation_for_testing_) {
            fail_next_input_context_preparation_for_testing_ = false;
            throw std::bad_alloc{};
        }
        const bool capture_allowed =
            event.type != InputType::PointerUp &&
            event.type != InputType::PointerCancel &&
            event.type != InputType::ContextMenu;
        const auto interaction_generation =
            dispatch_depth_ != 0U && event.pointer.id == active_input_pointer_id_
                ? active_input_pointer_generation_
                : pointer_interaction_dispatch_token(event.pointer.id);
        return InputContext{
            node.bounds,
            platform,
            unsafe_paint_invalidator(node),
            unsafe_layout_invalidator(node),
            [this,
             &node,
             pointer = event.pointer.id,
             capture_allowed,
             interaction_generation] {
                if (capture_allowed) {
                    set_pointer_capture(node, pointer, interaction_generation);
                }
            },
            [this, &node, pointer = event.pointer.id, interaction_generation] {
                release_pointer_capture(node, pointer, interaction_generation);
            }};
    }

    [[nodiscard]] EventResult send_prepared_to(
        Node& node, const InputEvent& event, InputContext& context) {
        try {
            return node.component->input(event, context);
        } catch (...) {
            if (event.type == InputType::PointerDown) {
                // Only unwind state created by this dispatch. A callback may
                // recursively deliver a newer PointerDown with the same ID;
                // its capture/interaction must survive the older frame.
                clear_pointer_capture_if_token(
                    event.pointer.id, active_input_dispatch_token_);
                end_pointer_interaction_if_token(
                    event.pointer.id, active_input_dispatch_token_);
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
                        auto context = prepare_input_context(*current, leave, platform);
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

    [[nodiscard]] EventResult route_without_pointer_capture(
        Node& target, const InputEvent& event, PlatformServices& platform) {
        // Capture permission is local to the InputContext for this event, so a
        // nested newer event with the same pointer ID is not accidentally
        // blocked by the outer cancellation/context-menu frame.
        return route_from(target, event, platform);
    }

    [[nodiscard]] EventResult cancel_pointer_capture(
        PlatformServices& platform, InputEvent event = InputEvent{}) {
        const auto pointer = event.pointer.id;
        auto* owner = pointer_capture_owner(pointer);
        const auto capture_token = pointer_capture_dispatch_token(pointer);
        const auto interaction_token = pointer_interaction_dispatch_token(pointer);
        if (!owner) {
            end_pointer_interaction_if_token(pointer, interaction_token);
            return EventResult::Ignored;
        }

        const auto index = pointer_capture_index(pointer);
        if (index == pointer_capture_count_) return EventResult::Ignored;
        // Preserve the legacy id=0 nested-cancel contract while making raw
        // tracked contacts idempotent under cancellation re-entry.
        if (pointer != 0U && pointer_captures_[index].cancelling) {
            return EventResult::Ignored;
        }
        pointer_captures_[index].cancelling = true;

        event.type = InputType::PointerCancel;
        try {
            const auto result = route_without_pointer_capture(*owner, event, platform);
            clear_pointer_capture_if_token(pointer, capture_token);
            end_pointer_interaction_if_token(pointer, interaction_token);
            return result;
        } catch (...) {
            clear_pointer_capture_if_token(pointer, capture_token);
            end_pointer_interaction_if_token(pointer, interaction_token);
            throw;
        }
    }

    [[nodiscard]] EventResult cancel_all_pointer_captures(PlatformServices& platform) {
        // Keep the historical id=0 nested-cancellation behavior: legacy mouse
        // callbacks may explicitly request another observable cancel. Tracked
        // raw contacts mark their in-flight capture as cancelling, so a nested
        // global pass skips only that contact while still draining siblings.
        const bool previous_barrier = pointer_capture_barrier_;
        pointer_capture_barrier_ = true;

        EventResult result = EventResult::Ignored;
        std::exception_ptr first_error;
        while (const auto pointer = first_pointer_capture_id()) {
            InputEvent event{};
            event.pointer.id = *pointer;
            try {
                const auto one = cancel_pointer_capture(platform, event);
                if (handled(one)) result = one;
            } catch (...) {
                if (!first_error) first_error = std::current_exception();
            }
        }

        clear_pointer_interactions();
        pointer_capture_barrier_ = previous_barrier;
        if (first_error) std::rethrow_exception(first_error);
        return result;
    }

    [[nodiscard]] EventResult send_to(
        Node& node, const InputEvent& event, PlatformServices& platform) {
        auto context = prepare_input_context(node, event, platform);
        return send_prepared_to(node, event, context);
    }

    std::unique_ptr<Node> root_;
    std::vector<Node*> focusables_;
    std::size_t focused_index_{};
    std::array<PointerCaptureState, kMaxTrackedPointers> pointer_captures_{};
    std::size_t pointer_capture_count_{};
    Node* pointer_hover_target_{};
    detail::OverlayService* overlay_service_{};
    std::array<PointerInteractionState, kMaxTrackedPointers> active_pointer_interactions_{};
    std::size_t active_pointer_interaction_count_{};
    bool pointer_interaction_active_{};
    std::uint64_t next_input_dispatch_token_{1U};
    std::uint64_t active_input_dispatch_token_{};
    PointerId active_input_pointer_id_{};
    std::uint64_t active_input_pointer_generation_{};
    bool pointer_capture_barrier_{};
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