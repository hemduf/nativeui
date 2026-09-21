    explicit Tree(std::unique_ptr<Node> root) : root_(std::move(root)) {
        focus_transition_stack_.reserve(4);
        pointer_hover_transition_stack_.reserve(4);
    }

    ~Tree() noexcept {
        try {
            unmount();
        } catch (...) {
        }
    }

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    void mount() {
        if (mounted_) return;
        reset_t125_semantic_recovery();
        LifecycleTransitionScope lifecycle{*this, LifecycleTransition::Mount, true};

        std::vector<Node*> staged_focusables;
        staged_focusables.swap(focusables_);
        staged_focusables.clear();

        focus_active_ = false;
        pointer_hover_target_ = nullptr;
        clear_pointer_interactions();
        focus_structure_dirty_ = true;
        pointer_passthrough_dynamic_root_ =
            dynamic_source(*root_) != nullptr &&
            !root_->component->pointer_targetable() &&
            !root_->component->focusable() &&
            !root_->component->clips_children() &&
            !root_->component->is_focus_scope();

        try {
            initialize_availability(*root_);
            availability_structure_dirty_ = false;
            mount_node(*root_, &staged_focusables);
            invalidate_layout_from(*root_);
        } catch (...) {
            try {
                unmount_node(*root_);
            } catch (...) {
            }
            staged_focusables.clear();
            focusables_.swap(staged_focusables);
            focusables_.clear();
            focus_active_ = false;
            clear_pointer_capture();
            pointer_hover_target_ = nullptr;
            clear_pointer_interactions();
            active_platform_ = nullptr;
            active_ = false;
            availability_structure_dirty_ = false;
            focus_structure_dirty_ = true;
            mounted_ = false;
            reset_t125_semantic_recovery();
            throw;
        }

        focusables_.swap(staged_focusables);
        mounted_ = true;
    }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints) const {
        if (lifecycle_transition_active()) return {};
        const_cast<Tree*>(this)->flush_dynamic_mutations();
        return measure_node(*root_, constraints);
    }

    void layout(Size viewport) {
        if (lifecycle_transition_active()) return;
        flush_dynamic_mutations();
        const bool size_changed = viewport.w != viewport_.w || viewport.h != viewport_.h;
        if (!size_changed && !layout_dirty_) return;

        auto transaction = begin_layout_publication(viewport);
        try {
            layout_node(*root_, viewport_rect());
        } catch (...) {
            rollback_layout_publication(std::move(transaction));
            throw;
        }
        commit_layout_publication();
    }

    void activate_focus(PlatformServices& platform) {
        LifecycleTransitionScope lifecycle{*this, LifecycleTransition::Activate, true};
        if (!mounted_) return;
        flush_dynamic_mutations();
        if (active_) return;

        active_ = true;
        try {
            activate_node(*root_);
        } catch (...) {
            try {
                deactivate_node(*root_);
            } catch (...) {
            }
            focus_active_ = false;
            clear_pointer_capture();
            pointer_hover_target_ = nullptr;
            clear_pointer_interactions();
            active_platform_ = nullptr;
            reset_focus_scope_cache(*root_);
            focus_structure_dirty_ = true;
            active_ = false;
            throw;
        }

        active_platform_ = &platform;
        try {
            sync_availability_structure(platform);
            focus_structure_dirty_ = true;
            sync_focus_structure(platform);
            if (!focus_active_) {
                if (auto* first = first_available_focusable()) set_focus(first, platform);
            }
            invalidate_paint(viewport_rect());
        } catch (...) {
            const auto activation_error = std::current_exception();
            lifecycle.release();
            try {
                deactivate_focus(platform);
            } catch (...) {
            }
            focus_active_ = false;
            clear_pointer_capture();
            pointer_hover_target_ = nullptr;
            clear_pointer_interactions();
            active_platform_ = nullptr;
            reset_focus_scope_cache(*root_);
            focus_structure_dirty_ = true;
            active_ = false;
            std::rethrow_exception(activation_error);
        }
    }

    void refresh_focus(PlatformServices& platform) {
        if (lifecycle_transition_active()) return;
        flush_dynamic_mutations();
        if (!active_) return;
        sync_availability_structure(platform);
        sync_focus_structure(platform);
        if (focusables_.empty() || !focus_active_) return;
        auto* node = focusables_[focused_index_];
        FocusContext context{
            node->bounds, platform, paint_invalidator(*node), layout_invalidator(*node)};
        node->component->focus_changed(true, context);
    }

    void deactivate_focus(PlatformServices& platform) {
        LifecycleTransitionScope lifecycle{*this, LifecycleTransition::Deactivate, true};
        std::exception_ptr first_error;
        auto capture_error = [&first_error](auto&& callback) {
            try {
                callback();
            } catch (...) {
                if (!first_error) first_error = std::current_exception();
            }
        };

        if (!active_) {
            reset_t125_semantic_recovery();
            return;
        }
        capture_error([&] { sync_availability_structure(platform); });

        InputEvent leave{};
        leave.type = InputType::PointerLeave;
        capture_error([&] { (void)clear_pointer_hover(leave, platform); });
        capture_error([&] { (void)cancel_all_pointer_captures(platform); });

        if (!focusables_.empty() && focus_active_) {
            auto* node = focusables_[focused_index_];
            FocusContext context{
                node->bounds, platform, paint_invalidator(*node), layout_invalidator(*node)};
            capture_error([&] { node->component->focus_changed(false, context); });
            capture_error([&] { invalidate_node_paint(*node); });
        }

        focus_active_ = false;
        clear_pointer_capture();
        pointer_hover_target_ = nullptr;
        clear_pointer_interactions();
        active_platform_ = nullptr;

        capture_error([&] { deactivate_node(*root_); });
        reset_focus_scope_cache(*root_);
        focus_structure_dirty_ = true;
        active_ = false;
        if (availability_structure_dirty_) {
            capture_error([&] { sync_availability_inactive(); });
        }
        capture_error([&] { invalidate_paint(viewport_rect()); });
        reset_t125_semantic_recovery();

        if (first_error) std::rethrow_exception(first_error);
    }

    void unmount() {
        LifecycleTransitionScope lifecycle{*this, LifecycleTransition::Unmount, true};
        if (!mounted_ || !root_) {
            reset_t125_semantic_recovery();
            return;
        }

        std::exception_ptr first_error;
        auto capture_error = [&first_error](auto&& callback) {
            try {
                callback();
            } catch (...) {
                if (!first_error) first_error = std::current_exception();
            }
        };

        focus_active_ = false;
        clear_pointer_capture();
        pointer_hover_target_ = nullptr;
        clear_pointer_interactions();
        active_platform_ = nullptr;
        if (active_) {
            capture_error([&] { deactivate_node(*root_); });
            active_ = false;
        }

        capture_error([&] { unmount_node(*root_); });
        focusables_.clear();
        mounted_ = false;
        reset_t125_semantic_recovery();

        if (first_error) std::rethrow_exception(first_error);
    }

    [[nodiscard]] bool dirty() const noexcept {
        return layout_dirty_ || !paint_dirty_.empty();
    }
    [[nodiscard]] bool layout_dirty() const noexcept { return layout_dirty_; }
    [[nodiscard]] bool paint_dirty() const noexcept { return !paint_dirty_.empty(); }
    [[nodiscard]] const std::vector<Rect>& dirty_regions() const noexcept {
        return paint_dirty_.rects();
    }
    [[nodiscard]] const std::string& structural_diagnostic() const noexcept {
        return structural_diagnostic_;
    }

    [[nodiscard]] std::optional<ComponentAvailability> component_availability(
        NodeId id) const noexcept {
        if (!root_) return std::nullopt;
        const auto* node = find_node(*root_, id);
        if (!node) return std::nullopt;
        return node->component->effective_availability();
    }

    void set_invalidation_callback(std::function<void(Rect)> callback) {
        invalidation_callback_ = std::move(callback);
        if (invalidation_callback_) {
            for (const auto rect : paint_dirty_.rects()) invalidation_callback_(rect);
        }
    }

    void set_invalidation_callback(std::function<void()> callback) {
        if (!callback) {
            set_invalidation_callback(std::function<void(Rect)>{});
            return;
        }
        set_invalidation_callback(
            [callback = std::move(callback)](Rect) mutable { callback(); });
    }

    void invalidate() { invalidate_paint(viewport_rect()); }
    void invalidate(Rect rect) { invalidate_paint(rect); }
    void invalidate_layout() { invalidate_layout_from(*root_); }

    void focus_next(PlatformServices& platform) {
        if (lifecycle_transition_active()) return;
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        move_focus(1, platform);
    }

    void focus_previous(PlatformServices& platform) {
        if (lifecycle_transition_active()) return;
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        move_focus(-1, platform);
    }

    void set_global_command_handler(std::function<EventResult(Command)> handler) {
        global_command_handler_ = std::move(handler);
    }

    void set_global_key_down_handler(
        std::function<EventResult(const InputEvent&)> handler) {
        global_key_down_handler_ = std::move(handler);
    }

    [[nodiscard]] std::size_t pointer_interaction_index(
        PointerId pointer) const noexcept {
        for (std::size_t i = 0; i < active_pointer_interaction_count_; ++i) {
            if (active_pointer_interactions_[i].pointer == pointer) return i;
        }
        return active_pointer_interaction_count_;
    }

    [[nodiscard]] std::uint64_t pointer_interaction_dispatch_token(
        PointerId pointer) const noexcept {
        const auto index = pointer_interaction_index(pointer);
        return index == active_pointer_interaction_count_
            ? 0U
            : active_pointer_interactions_[index].dispatch_token;
    }

    void begin_pointer_interaction(PointerId pointer = 0U) noexcept {
        const auto index = pointer_interaction_index(pointer);
        if (index != active_pointer_interaction_count_) {
            active_pointer_interactions_[index].dispatch_token =
                active_input_dispatch_token_;
            pointer_interaction_active_ = true;
            return;
        }
        if (active_pointer_interaction_count_ < active_pointer_interactions_.size()) {
            active_pointer_interactions_[active_pointer_interaction_count_++] = {
                pointer, active_input_dispatch_token_};
        }
        pointer_interaction_active_ = active_pointer_interaction_count_ != 0U;
    }

    void end_pointer_interaction(PointerId pointer = 0U) noexcept {
        const auto index = pointer_interaction_index(pointer);
        if (index == active_pointer_interaction_count_) return;
        --active_pointer_interaction_count_;
        if (index != active_pointer_interaction_count_) {
            active_pointer_interactions_[index] =
                active_pointer_interactions_[active_pointer_interaction_count_];
        }
        active_pointer_interactions_[active_pointer_interaction_count_] = {};
        pointer_interaction_active_ = active_pointer_interaction_count_ != 0U;
    }

    void end_pointer_interaction_if_token(
        PointerId pointer, std::uint64_t dispatch_token) noexcept {
        const auto index = pointer_interaction_index(pointer);
        if (index == active_pointer_interaction_count_ ||
            active_pointer_interactions_[index].dispatch_token != dispatch_token) {
            return;
        }
        end_pointer_interaction(pointer);
    }

    void clear_pointer_interactions() noexcept {
        for (std::size_t i = 0; i < active_pointer_interaction_count_; ++i) {
            active_pointer_interactions_[i] = {};
        }
        active_pointer_interaction_count_ = 0U;
        pointer_interaction_active_ = false;
    }

    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        if (lifecycle_transition_active()) return EventResult::Ignored;
        const bool is_drop = event.type == InputType::DropOffer || event.type == InputType::DropData;
        if (is_drop ? !mounted_ : !active_) return EventResult::Ignored;

        resume_pointer_hover_transitions(platform);
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        if (active_) sync_focus_structure(platform);

        const auto previous_dispatch_depth = dispatch_depth_;
        const auto previous_input_dispatch_token = active_input_dispatch_token_;
        if (next_input_dispatch_token_ == 0U) next_input_dispatch_token_ = 1U;
        active_input_dispatch_token_ = next_input_dispatch_token_++;
        if (next_input_dispatch_token_ == 0U) next_input_dispatch_token_ = 1U;

        dispatch_depth_ = previous_dispatch_depth + 1;
        bool pointer_route_committed = false;
        auto finish = [this,
                       &platform,
                       previous_dispatch_depth,
                       previous_input_dispatch_token](EventResult result) {
            active_input_dispatch_token_ = previous_input_dispatch_token;
            dispatch_depth_ = previous_dispatch_depth;
            if (previous_dispatch_depth == 0) {
                flush_dynamic_mutations();
                sync_availability_structure(platform);
                if (active_) sync_focus_structure(platform);
            }
            return result;
        };

        try {
            Node& pointer_hit_root =
                pointer_passthrough_dynamic_root_ && root_->children.size() == 1
                    ? *root_->children.front()
                    : *root_;

            auto hit_test_pointer = [&](auto&& self, Node& node, Point point, Rect inherited_clip) -> Node* {
                if (inherited_clip.empty() || !inherited_clip.contains(point) ||
                    dynamic_teardown_quarantines(&node) ||
                    !node.component->effective_availability().interactive()) {
                    return nullptr;
                }
                if (node.component->is_focus_scope() && !node.component->focus_scope_active()) {
                    return nullptr;
                }

                Rect child_clip = inherited_clip;
                if (node.component->clips_children()) {
                    child_clip = intersect(inherited_clip, node.bounds);
                }
                if (!child_clip.empty()) {
                    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                        if (auto* result = self(self, **it, point, child_clip)) return result;
                    }
                }
                return node.bounds.contains(point) && node.component->pointer_targetable() ? &node : nullptr;
            };

            auto focus_nearest_ancestor = [this, &platform](Node* target) {
                for (auto* current = target; current; current = current->parent) {
                    if (!current->component->focusable()) continue;
                    set_focus(current, platform);
                    return;
                }
            };

            if (event.type == InputType::KeyDown && event.key == Key::Tab) {
                if (!focus_active_) return finish(EventResult::Ignored);
                event.shift ? move_focus_group_aware(-1, platform)
                            : move_focus_group_aware(1, platform);
                return finish(EventResult::Handled);
            }

            if (event.type == InputType::KeyDown && focus_active_) {
                int group_direction = 0;
                if (event.key == Key::Left || event.key == Key::Up) group_direction = -1;
                if (event.key == Key::Right || event.key == Key::Down) group_direction = 1;
                if (group_direction != 0 && move_focus_in_current_group(group_direction, platform)) {
                    return finish(EventResult::Handled);
                }
            }

            if (event.type == InputType::Command || command_from_shortcut(event) != Command::None) {
                InputEvent command_event = event;
                if (command_event.type != InputType::Command) {
                    command_event.type = InputType::Command;
                    command_event.command = command_from_shortcut(event);
                    command_event.key = Key::None;
                    command_event.text.clear();
                }
                if (command_event.command == Command::None) return finish(EventResult::Ignored);

                EventResult result = EventResult::Ignored;
                if (focus_active_ && !focusables_.empty()) {
                    result = route_from(*focusables_[focused_index_], command_event, platform);
                }
                if (!handled(result) && global_command_handler_) {
                    result = global_command_handler_(command_event.command);
                }
                return finish(result);
            }

            if (event.type == InputType::PointerLeave) {
                const bool changed = clear_pointer_hover(event, platform);
                return finish(changed ? EventResult::Handled : EventResult::Ignored);
            }

            if (event.type == InputType::PointerCancel) {
                // cancel_pointer_capture() owns terminal cleanup even if the
                // callback throws. Mark this before entering user code so the
                // generic unwind cannot erase a newer reentrant contact.
                pointer_route_committed = true;
                const auto result = cancel_pointer_capture(platform, event);
                return finish(result);
            }

            if (event.type == InputType::PointerDown) {
                if (pointer_capture_owner(event.pointer.id)) {
                    // If stale-capture cancellation throws, that helper has
                    // already restored its own state and must not be followed
                    // by generic cleanup that could erase a nested newer event.
                    pointer_route_committed = true;
                    (void)cancel_pointer_capture(platform, event);
                    pointer_route_committed = false;
                }
                begin_pointer_interaction(event.pointer.id);

                auto* target = hit_test_pointer(
                    hit_test_pointer, pointer_hit_root, event.position, viewport_rect());
                if (event.pointer.hover_capable()) {
                    (void)update_pointer_hover_target(target, event, platform);
                }
                if (target) {
                    focus_nearest_ancestor(target);
                    const auto result = route_from(*target, event, platform);
                    pointer_route_committed = true;
                    return finish(result);
                }
                pointer_route_committed = true;
                return finish(EventResult::Ignored);
            }

            if (event.type == InputType::ContextMenu) {
                // Same targeting as a press but without keyboard focus. The
                // existing interaction is terminal for this frame, but a
                // callback may recursively begin a newer interaction with the
                // same ID and that newer state must survive.
                const auto interaction_token =
                    pointer_interaction_dispatch_token(event.pointer.id);
                pointer_route_committed = true;
                if (pointer_capture_owner(event.pointer.id)) {
                    (void)cancel_pointer_capture(platform, event);
                } else {
                    end_pointer_interaction_if_token(
                        event.pointer.id, interaction_token);
                }
                auto* target = hit_test_pointer(
                    hit_test_pointer, pointer_hit_root, event.position, viewport_rect());
                if (target) {
                    return finish(route_without_pointer_capture(*target, event, platform));
                }
                return finish(EventResult::Ignored);
            }

            if (event.type == InputType::PointerMove || event.type == InputType::PointerWheel) {
                auto* target = hit_test_pointer(
                    hit_test_pointer, pointer_hit_root, event.position, viewport_rect());
                if (event.pointer.hover_capable()) {
                    (void)update_pointer_hover_target(target, event, platform);
                }
                if (auto* owner = pointer_capture_owner(event.pointer.id)) {
                    return finish(route_from(*owner, event, platform));
                }
                if (target) return finish(route_from(*target, event, platform));
                return finish(EventResult::Ignored);
            }

            if (event.type == InputType::PointerUp) {
                auto* const owner = pointer_capture_owner(event.pointer.id);
                const auto capture_token =
                    pointer_capture_dispatch_token(event.pointer.id);
                const auto interaction_token =
                    pointer_interaction_dispatch_token(event.pointer.id);

                // This frame owns cleanup of the state that existed when Up
                // arrived. Perform only token-matched cleanup so callbacks may
                // recursively start a newer contact with the same stable ID.
                pointer_route_committed = true;
                try {
                    auto* target = hit_test_pointer(
                        hit_test_pointer, pointer_hit_root, event.position, viewport_rect());
                    if (event.pointer.hover_capable()) {
                        (void)update_pointer_hover_target(target, event, platform);
                    }

                    EventResult result = EventResult::Ignored;
                    if (owner) {
                        result = route_from(*owner, event, platform);
                    } else if (target) {
                        result = route_from(*target, event, platform);
                    }

                    clear_pointer_capture_if_token(
                        event.pointer.id, capture_token);
                    end_pointer_interaction_if_token(
                        event.pointer.id, interaction_token);
                    return finish(result);
                } catch (...) {
                    clear_pointer_capture_if_token(
                        event.pointer.id, capture_token);
                    end_pointer_interaction_if_token(
                        event.pointer.id, interaction_token);
                    throw;
                }
            }

            if (is_drop) {
                ensure_layout();
                if (auto* target = hit_test_any(*root_, event.position, viewport_rect())) {
                    return finish(route_from(*target, event, platform));
                }
                return finish(EventResult::Ignored);
            }

            if ((event.type == InputType::KeyDown || event.type == InputType::KeyUp ||
                 event.type == InputType::TextInput || event.type == InputType::Composition ||
                 event.type == InputType::Tick) &&
                focus_active_ && !focusables_.empty()) {
                auto result = route_from(*focusables_[focused_index_], event, platform);
                if (event.type == InputType::KeyDown && !handled(result) &&
                    global_key_down_handler_) {
                    result = global_key_down_handler_(event);
                }
                return finish(result);
            }

            if (event.type == InputType::KeyDown && global_key_down_handler_) {
                return finish(global_key_down_handler_(event));
            }

            return finish(EventResult::Ignored);
        } catch (...) {
            if (!pointer_route_committed &&
                event.type == InputType::PointerDown) {
                clear_pointer_capture_if_token(
                    event.pointer.id, active_input_dispatch_token_);
                end_pointer_interaction_if_token(
                    event.pointer.id, active_input_dispatch_token_);
            }
            active_input_dispatch_token_ = previous_input_dispatch_token;
            dispatch_depth_ = previous_dispatch_depth;
            throw;
        }
    }

    EventResult cancel_pointer(PlatformServices& platform) {
        if (lifecycle_transition_active()) return EventResult::Ignored;
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        return cancel_all_pointer_captures(platform);
    }

    void paint(SkCanvas& canvas, PlatformServices& platform) {
        if (lifecycle_transition_active()) return;
        flush_dynamic_mutations();
        if (active_) {
            sync_availability_structure(platform);
            sync_focus_structure(platform);
        }
        ensure_layout();

        Painter painter{canvas};
        paint_node(*root_, painter, platform, viewport_rect());
        paint_dirty_.clear();
    }
