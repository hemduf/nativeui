    explicit Tree(std::unique_ptr<Node> root) : root_(std::move(root)) {}
    ~Tree() { unmount(); }

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    void mount() {
        if (mounted_) return;
        mounted_ = true;
        focusables_.clear();
        focus_active_ = false;
        focus_structure_dirty_ = true;
        mount_node(*root_);
        invalidate_layout_from(*root_);
    }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints) const {
        return measure_node(*root_, constraints);
    }

    /// Immediately recompute layout for an externally supplied viewport (for
    /// example a window resize), and dirty the complete new viewport.
    void layout(Size viewport) {
        const bool size_changed = viewport.w != viewport_.w || viewport.h != viewport_.h;
        if (!size_changed && !layout_dirty_) return;

        viewport_ = viewport;
        paint_dirty_.clear();
        layout_node(*root_, viewport_rect());
        layout_dirty_ = false;
        invalidate_paint(viewport_rect());
    }

    void activate_focus(PlatformServices& platform) {
        if (active_) return;
        active_ = true;
        activate_node(*root_);
        focus_structure_dirty_ = true;
        sync_focus_structure(platform);
        if (!focus_active_) {
            if (auto* first = first_available_focusable()) set_focus(first, platform);
        }
        invalidate_paint(viewport_rect());
    }

    void refresh_focus(PlatformServices& platform) {
        if (!active_) return;
        sync_focus_structure(platform);
        if (focusables_.empty() || !focus_active_) return;
        auto* node = focusables_[focused_index_];
        FocusContext context{
            node->bounds, platform, paint_invalidator(*node), layout_invalidator(*node)};
        node->component->focus_changed(true, context);
    }

    void deactivate_focus(PlatformServices& platform) {
        if (!active_) return;

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
        deactivate_node(*root_);
        reset_focus_scope_cache(*root_);
        focus_structure_dirty_ = true;
        active_ = false;
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
        move_focus(1, platform);
    }

    void focus_previous(PlatformServices& platform) {
        move_focus(-1, platform);
    }

    void set_global_command_handler(std::function<EventResult(Command)> handler) {
        global_command_handler_ = std::move(handler);
    }

    /// Route one input event according to the retained-mode targeting rules:
    ///
    /// - Tab/Shift+Tab is owned by the tree and never double-delivered;
    /// - key/text/tick events target the focused node while focus is active;
    /// - PointerDown cancels an older capture, then hit-tests/focuses a new target;
    /// - PointerMove/PointerWheel route to the capture owner first;
    /// - PointerUp routes to the capture owner then releases capture automatically;
    /// - PointerCancel is delivered exactly once to the capture owner and clears capture;
    /// - external drop offers/data hit-test a mounted tree even without window focus;
    /// - ignored target events bubble through ancestors until handled or root.
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        // An external drag source (for example Finder) owns keyboard focus.
        // Drop targeting must remain available without activating this tree
        // or restoring keyboard focus/text input as a side effect.
        if (event.type == InputType::DropOffer || event.type == InputType::DropData) {
            if (!mounted_) return EventResult::Ignored;
            ensure_layout();
            if (auto* target = hit_test_any(*root_, event.position, viewport_rect())) {
                return route_from(*target, event, platform);
            }
            return EventResult::Ignored;
        }

        if (!active_) return EventResult::Ignored;
        sync_focus_structure(platform);

        if (event.type == InputType::KeyDown && event.key == Key::Tab) {
            if (!focus_active_) return EventResult::Ignored;
            event.shift ? focus_previous(platform) : focus_next(platform);
            return EventResult::Handled;
        }

        if (event.type == InputType::Command || command_from_shortcut(event) != Command::None) {
            InputEvent command_event = event;
            if (command_event.type != InputType::Command) {
                command_event.type = InputType::Command;
                command_event.command = command_from_shortcut(event);
                command_event.key = Key::None;
                command_event.text.clear();
            }

            if (command_event.command == Command::None) return EventResult::Ignored;

            EventResult result = EventResult::Ignored;
            if (focus_active_ && !focusables_.empty()) {
                result = route_from(*focusables_[focused_index_], command_event, platform);
            }
            if (!handled(result) && global_command_handler_) {
                result = global_command_handler_(command_event.command);
            }
            return result;
        }

        if (event.type == InputType::PointerCancel) {
            return cancel_pointer_capture(platform, event);
        }

        if (event.type == InputType::PointerDown) {
            // A new pointer-down starts a new interaction. Never leave an old
            // owner captured if a previous interaction missed its PointerUp.
            if (pointer_capture_.node) (void)cancel_pointer_capture(platform);

            if (auto* target = hit_test_focusable(*root_, event.position, viewport_rect())) {
                set_focus(target, platform);
                return route_from(*target, event, platform);
            }
            return EventResult::Ignored;
        }

        if (event.type == InputType::PointerMove || event.type == InputType::PointerWheel) {
            if (auto* owner = pointer_capture_owner()) {
                return route_from(*owner, event, platform);
            }
            if (auto* target = hit_test_focusable(*root_, event.position, viewport_rect())) {
                return route_from(*target, event, platform);
            }
            return EventResult::Ignored;
        }

        if (event.type == InputType::PointerUp) {
            if (auto* owner = pointer_capture_owner()) {
                const auto result = route_from(*owner, event, platform);
                clear_pointer_capture();
                return result;
            }
            if (auto* target = hit_test_focusable(*root_, event.position, viewport_rect())) {
                return route_from(*target, event, platform);
            }
            return EventResult::Ignored;
        }

        if ((event.type == InputType::KeyDown || event.type == InputType::KeyUp ||
             event.type == InputType::TextInput || event.type == InputType::Tick) &&
            focus_active_ && !focusables_.empty()) {
            return route_from(*focusables_[focused_index_], event, platform);
        }

        return EventResult::Ignored;
    }

    /// Explicitly cancel the active pointer capture, if any. The current owner
    /// receives one PointerCancel event before capture is cleared.
    EventResult cancel_pointer(PlatformServices& platform) {
        return cancel_pointer_capture(platform);
    }

    void paint(SkCanvas& canvas, PlatformServices& platform) {
        if (active_) sync_focus_structure(platform);
        ensure_layout();

        Painter painter{canvas};
        painter.fill_rounded_rect(
            Rect{0.0f, 0.0f, viewport_.w, viewport_.h},
            0.0f,
            colors::background);

        paint_node(*root_, painter, platform, viewport_rect());

        painter.text(
            Point{24.0f, std::max(24.0f, viewport_.h - 22.0f)},
            "TAB / SHIFT+TAB focus    arrows edit    mouse select/drag    CMD/CTRL+C/X/V/Z",
            12.0f,
            colors::textMuted,
            TextAlign::Left);

        paint_dirty_.clear();
    }
