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
        if (cancelling_pointer_capture_) return;
        pointer_capture_ = PointerCaptureState{&node, node.id};
    }

    void release_pointer_capture(Node& node) noexcept {
        if (pointer_capture_.node == &node && pointer_capture_.owner == node.id) {
            clear_pointer_capture();
        }
    }

    void clear_pointer_capture() noexcept {
        pointer_capture_ = {};
    }

    [[nodiscard]] EventResult cancel_pointer_capture(
        PlatformServices& platform, InputEvent event = InputEvent{}) {
        auto* owner = pointer_capture_owner();
        if (!owner) return EventResult::Ignored;

        event.type = InputType::PointerCancel;
        // Keep the owner recorded while routing so release_pointer() remains
        // idempotent, but prevent a cancellation handler from re-capturing.
        cancelling_pointer_capture_ = true;
        const auto result = route_from(*owner, event, platform);
        cancelling_pointer_capture_ = false;
        clear_pointer_capture();
        return result;
    }

    [[nodiscard]] EventResult send_to(
        Node& node, const InputEvent& event, PlatformServices& platform) {
        InputContext context{
            node.bounds,
            platform,
            paint_invalidator(node),
            layout_invalidator(node),
            [this, &node] { set_pointer_capture(node); },
            [this, &node] { release_pointer_capture(node); }};
        return node.component->input(event, context);
    }

    std::unique_ptr<Node> root_;
    std::vector<Node*> focusables_;
    std::size_t focused_index_{};
    PointerCaptureState pointer_capture_{};
    bool cancelling_pointer_capture_{};
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