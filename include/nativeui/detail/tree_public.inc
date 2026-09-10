    explicit Tree(std::unique_ptr<Node> root) : root_(std::move(root)) {}
    ~Tree() { unmount(); }

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    void mount() {
        if (mounted_) return;
        mounted_ = true;
        focusables_.clear();
        focus_active_ = false;
        pointer_hover_target_ = nullptr;
        focus_structure_dirty_ = true;
        initialize_availability(*root_);
        availability_structure_dirty_ = false;
        mount_node(*root_);
        invalidate_layout_from(*root_);
    }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints) const {
        const_cast<Tree*>(this)->flush_dynamic_mutations();
        return measure_node(*root_, constraints);
    }

    /// Immediately recompute layout for an externally supplied viewport (for
    /// example a window resize), and dirty the complete new viewport.
    void layout(Size viewport) {
        flush_dynamic_mutations();
        const bool size_changed = viewport.w != viewport_.w || viewport.h != viewport_.h;
        if (!size_changed && !layout_dirty_) return;

        viewport_ = viewport;
        paint_dirty_.clear();
        layout_node(*root_, viewport_rect());
        layout_dirty_ = false;
        invalidate_paint(viewport_rect());
    }

    void activate_focus(PlatformServices& platform) {
        flush_dynamic_mutations();
        if (active_) return;
        active_ = true;
        activate_node(*root_);
        active_platform_ = &platform;
        sync_availability_structure(platform);
        focus_structure_dirty_ = true;
        sync_focus_structure(platform);
        if (!focus_active_) {
            if (auto* first = first_available_focusable()) set_focus(first, platform);
        }
        invalidate_paint(viewport_rect());
    }

    void refresh_focus(PlatformServices& platform) {
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
        flush_dynamic_mutations();
        if (!active_) return;
        sync_availability_structure(platform);

        // Hover and capture callbacks must run while components and platform
        // services are still active. Hover is presentation-only and is cleared
        // independently from any in-flight captured interaction.
        InputEvent leave{};
        leave.type = InputType::PointerLeave;
        (void)clear_pointer_hover(leave, platform);

        // Cancel any in-flight pointer interaction while components and
        // platform services are still active. This is the deterministic
        // lifecycle boundary used for focus loss and view teardown.
        (void)cancel_pointer_capture(platform);

        if (!focusables_.empty() && focus_active_) {
            auto* node = focusables_[focused_index_];
            FocusContext context{
                node->bounds, platform, paint_invalidator(*node), layout_invalidator(*node)};
            node->component->focus_changed(false, context);
            invalidate_node_paint(*node);
        }

        focus_active_ = false;
        clear_pointer_capture();
        pointer_hover_target_ = nullptr;
        active_platform_ = nullptr;
        deactivate_node(*root_);
        reset_focus_scope_cache(*root_);
        focus_structure_dirty_ = true;
        active_ = false;
        if (availability_structure_dirty_) sync_availability_inactive();
        invalidate_paint(viewport_rect());
    }

    void unmount() {
        if (!mounted_ || !root_) return;

        // Normal platform integrations deactivate before unmount so focused
        // controls can release platform services. This fallback still runs the
        // platform-neutral component lifecycle if a caller destroys an active
        // tree directly.
        focus_active_ = false;
        clear_pointer_capture();
        pointer_hover_target_ = nullptr;
        active_platform_ = nullptr;
        if (active_) {
            deactivate_node(*root_);
            active_ = false;
        }

        unmount_node(*root_);
        focusables_.clear();
        mounted_ = false;
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

    /// Install a logical-coordinate exposure callback. Existing accumulated
    /// dirty rectangles are replayed so attaching a platform after initial
    /// layout still schedules the first frame.
    void set_invalidation_callback(std::function<void(Rect)> callback) {
        invalidation_callback_ = std::move(callback);
        if (invalidation_callback_) {
            for (const auto rect : paint_dirty_.rects()) invalidation_callback_(rect);
        }
    }

    /// Compatibility overload for callers that only support whole-view redraw.
    void set_invalidation_callback(std::function<void()> callback) {
        if (!callback) {
            set_invalidation_callback(std::function<void(Rect)>{});
            return;
        }
        set_invalidation_callback(
            [callback = std::move(callback)](Rect) mutable { callback(); });
    }

    /// Repaint the whole viewport without recomputing layout.
    void invalidate() { invalidate_paint(viewport_rect()); }
    /// Repaint a logical rectangle, clipped to the current viewport.
    void invalidate(Rect rect) { invalidate_paint(rect); }
    /// Recompute layout from the root before the next paint.
    void invalidate_layout() { invalidate_layout_from(*root_); }

    void focus_next(PlatformServices& platform) {
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        move_focus(1, platform);
    }

    void focus_previous(PlatformServices& platform) {
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        move_focus(-1, platform);
    }

    void set_global_command_handler(std::function<EventResult(Command)> handler) {
        global_command_handler_ = std::move(handler);
    }

    /// Route one input event according to the retained-mode targeting rules:
    ///
    /// - Tab/Shift+Tab is owned by the tree and never double-delivered;
    /// - key/text/composition/tick events target the focused node while focus is active;
    /// - PointerDown cancels an older capture, hit-tests the deepest interactive visual node,
    ///   focuses its nearest focusable ancestor when one exists, then routes from the visual target;
    /// - PointerMove/PointerWheel update the retained hover route, then route to the capture owner
    ///   first when present, otherwise the current visual target;
    /// - PointerLeave clears the previous retained hover route without cancelling capture;
    /// - PointerUp updates hover, routes to the capture owner, then releases capture automatically;
    /// - PointerCancel is delivered exactly once to the capture owner and clears capture;
    /// - external drop offers/data hit-test a mounted tree even without window focus;
    /// - ignored target events bubble through ancestors until handled or root.
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        // An external drag source (for example Finder) owns keyboard focus.
        // Drop targeting must remain available without activating this tree
        // or restoring keyboard focus/text input as a side effect.
        const bool is_drop = event.type == InputType::DropOffer || event.type == InputType::DropData;
        if (is_drop ? !mounted_ : !active_) return EventResult::Ignored;
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        if (active_) sync_focus_structure(platform);

        ++dispatch_depth_;
        auto finish = [this, &platform](EventResult result) {
            if (dispatch_depth_ > 0) --dispatch_depth_;
            if (dispatch_depth_ == 0) {
                flush_dynamic_mutations();
                sync_availability_structure(platform);
                if (active_) sync_focus_structure(platform);
            }
            return result;
        };

        // Pointer targeting is intentionally independent from keyboard focus.
        // Reverse child order matches paint stacking: later/overlay children are
        // input-frontmost. Inert visual/layout nodes retain the historic behavior
        // of not blocking an interactive sibling below them unless they explicitly
        // opt into pointer targetability. Inactive focus scopes remain unavailable.
        auto hit_test_pointer = [&](auto&& self, Node& node, Point point, Rect inherited_clip) -> Node* {
            if (inherited_clip.empty() || !inherited_clip.contains(point) ||
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
            return finish(cancel_pointer_capture(platform, event));
        }

        if (event.type == InputType::PointerDown) {
            // A new pointer-down starts a new interaction. Never leave an old
            // owner captured if a previous interaction missed its PointerUp.
            if (pointer_capture_.node) (void)cancel_pointer_capture(platform);

            auto* target = hit_test_pointer(
                hit_test_pointer, *root_, event.position, viewport_rect());
            (void)update_pointer_hover_target(target, event, platform);
            if (target) {
                focus_nearest_ancestor(target);
                return finish(route_from(*target, event, platform));
            }
            return finish(EventResult::Ignored);
        }

        if (event.type == InputType::PointerMove || event.type == InputType::PointerWheel) {
            auto* target = hit_test_pointer(
                hit_test_pointer, *root_, event.position, viewport_rect());
            (void)update_pointer_hover_target(target, event, platform);
            if (auto* owner = pointer_capture_owner()) {
                return finish(route_from(*owner, event, platform));
            }
            if (target) {
                return finish(route_from(*target, event, platform));
            }
            return finish(EventResult::Ignored);
        }

        if (event.type == InputType::PointerUp) {
            auto* target = hit_test_pointer(
                hit_test_pointer, *root_, event.position, viewport_rect());
            (void)update_pointer_hover_target(target, event, platform);
            if (auto* owner = pointer_capture_owner()) {
                const auto result = route_from(*owner, event, platform);
                clear_pointer_capture();
                return finish(result);
            }
            if (target) {
                return finish(route_from(*target, event, platform));
            }
            return finish(EventResult::Ignored);
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
            return finish(route_from(*focusables_[focused_index_], event, platform));
        }

        return finish(EventResult::Ignored);
    }

    /// Explicitly cancel the active pointer capture, if any. The current owner
    /// receives one PointerCancel event before capture is cleared.
    EventResult cancel_pointer(PlatformServices& platform) {
        flush_dynamic_mutations();
        sync_availability_structure(platform);
        return cancel_pointer_capture(platform);
    }

    void paint(SkCanvas& canvas, PlatformServices& platform) {
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
