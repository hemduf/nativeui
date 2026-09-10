class ScrollViewComponent final : public Component {
public:
    ScrollViewComponent(ScrollState& state, bool pointer_pan)
        : state_(&state), pointer_pan_(pointer_pan) {}

    [[nodiscard]] bool clips_children() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (event.type == InputType::PointerWheel) {
            const auto before = state_->offset();
            state_->scroll_by(event.delta);
            return same(before, state_->offset()) ? EventResult::Ignored : EventResult::Handled;
        }

        if (!pointer_pan_) return EventResult::Ignored;

        switch (event.type) {
        case InputType::PointerDown:
            pan_active_ = true;
            pan_origin_ = event.position;
            offset_origin_ = state_->offset();
            context.capture_pointer();
            return EventResult::Handled;

        case InputType::PointerMove:
            if (!pan_active_) return EventResult::Ignored;
            state_->set_offset(Point{
                offset_origin_.x - (event.position.x - pan_origin_.x),
                offset_origin_.y - (event.position.y - pan_origin_.y)});
            return EventResult::Handled;

        case InputType::PointerUp:
        case InputType::PointerCancel:
            if (!pan_active_) return EventResult::Ignored;
            pan_active_ = false;
            context.release_pointer();
            return EventResult::Handled;

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override { pan_active_ = false; }

    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] static bool same(Point a, Point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }

    ScrollState* state_{};
    bool pointer_pan_{};
    bool pan_active_{};
    Point pan_origin_{};
    Point offset_origin_{};
};

class ScrollView {
public:
    template <class Child>
    ScrollView(ScrollState& state, Child&& child)
        : state_(&state), content_(make_spec(std::forward<Child>(child))) {}

    ScrollView&& pointer_pan(bool enabled = true) && {
        pointer_pan_ = enabled;
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;

        std::vector<Spec> scroll_children;
        scroll_children.push_back(std::move(content_));
        Spec scroll{
            [state] { return std::make_unique<ScrollComponent>(*state); },
            std::move(scroll_children)};

        std::vector<Spec> children;
        children.push_back(std::move(scroll));
        const bool pointer_pan = pointer_pan_;
        return Spec{
            [state, pointer_pan] {
                return std::make_unique<ScrollViewComponent>(*state, pointer_pan);
            },
            std::move(children)};
    }

private:
    ScrollState* state_{};
    Spec content_;
    bool pointer_pan_{};
};
