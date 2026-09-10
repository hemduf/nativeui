enum class ScrollAlignment {
    Nearest,
    Start,
    Center,
    End,
};

namespace detail {

[[nodiscard]] inline bool scroll_axis_horizontal(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both;
}

[[nodiscard]] inline bool scroll_axis_vertical(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Vertical || axis == ScrollAxis::Both;
}

[[nodiscard]] inline float ensure_visible_axis(
    float current,
    float viewport_extent,
    float descendant_start,
    float descendant_extent,
    ScrollAlignment alignment) noexcept {
    if (!std::isfinite(current) || !std::isfinite(viewport_extent) || viewport_extent <= 0.0f ||
        !std::isfinite(descendant_start) || !std::isfinite(descendant_extent) ||
        descendant_extent < 0.0f) {
        return current;
    }

    const float descendant_end = descendant_start + descendant_extent;
    if (!std::isfinite(descendant_end)) return current;

    switch (alignment) {
    case ScrollAlignment::Start:
        return descendant_start;
    case ScrollAlignment::Center:
        return descendant_start - (viewport_extent - descendant_extent) * 0.5f;
    case ScrollAlignment::End:
        return descendant_end - viewport_extent;
    case ScrollAlignment::Nearest:
        if (descendant_extent > viewport_extent) return descendant_start;
        if (descendant_start < current) return descendant_start;
        if (descendant_end > current + viewport_extent) {
            return descendant_end - viewport_extent;
        }
        return current;
    }
    return current;
}

struct ScrollbarAxisGeometry {
    bool visible{};
    Rect track{};
    Rect thumb{};
};

struct ScrollViewBars {
    ScrollbarAxisGeometry horizontal{};
    ScrollbarAxisGeometry vertical{};
};

[[nodiscard]] inline ScrollbarAxisGeometry make_horizontal_scrollbar(
    const ScrollState& state,
    Rect viewport,
    bool vertical_visible,
    float thickness,
    float min_thumb) noexcept {
    ScrollbarAxisGeometry result;
    const auto content = state.content_size();
    const auto metrics_viewport = state.viewport_size();
    const auto maximum = state.max_offset();
    if (!scroll_axis_horizontal(state.axis()) || !(content.w > metrics_viewport.w) ||
        !(maximum.x > 0.0f)) {
        return result;
    }

    const float track_length = std::max(0.0f, viewport.w - (vertical_visible ? thickness : 0.0f));
    if (!(track_length > 0.0f)) return result;

    result.visible = true;
    result.track = Rect{viewport.x, viewport.y + std::max(0.0f, viewport.h - thickness),
                        track_length, thickness};
    const float requested = track_length * (metrics_viewport.w / content.w);
    const float thumb_length = std::clamp(std::max(min_thumb, requested), 0.0f, track_length);
    const float travel = std::max(0.0f, track_length - thumb_length);
    const float fraction = std::clamp(state.offset().x / maximum.x, 0.0f, 1.0f);
    result.thumb = Rect{result.track.x + travel * fraction, result.track.y,
                        thumb_length, thickness};
    return result;
}

[[nodiscard]] inline ScrollbarAxisGeometry make_vertical_scrollbar(
    const ScrollState& state,
    Rect viewport,
    bool horizontal_visible,
    float thickness,
    float min_thumb) noexcept {
    ScrollbarAxisGeometry result;
    const auto content = state.content_size();
    const auto metrics_viewport = state.viewport_size();
    const auto maximum = state.max_offset();
    if (!scroll_axis_vertical(state.axis()) || !(content.h > metrics_viewport.h) ||
        !(maximum.y > 0.0f)) {
        return result;
    }

    const float track_length = std::max(0.0f, viewport.h - (horizontal_visible ? thickness : 0.0f));
    if (!(track_length > 0.0f)) return result;

    result.visible = true;
    result.track = Rect{viewport.x + std::max(0.0f, viewport.w - thickness), viewport.y,
                        thickness, track_length};
    const float requested = track_length * (metrics_viewport.h / content.h);
    const float thumb_length = std::clamp(std::max(min_thumb, requested), 0.0f, track_length);
    const float travel = std::max(0.0f, track_length - thumb_length);
    const float fraction = std::clamp(state.offset().y / maximum.y, 0.0f, 1.0f);
    result.thumb = Rect{result.track.x, result.track.y + travel * fraction,
                        thickness, thumb_length};
    return result;
}

[[nodiscard]] inline ScrollViewBars scroll_view_bars(
    const ScrollState& state,
    Rect viewport,
    float thickness = 8.0f,
    float min_thumb = 18.0f) noexcept {
    thickness = std::isfinite(thickness) ? std::max(0.0f, thickness) : 0.0f;
    min_thumb = std::isfinite(min_thumb) ? std::max(0.0f, min_thumb) : 0.0f;

    const auto content = state.content_size();
    const auto metrics_viewport = state.viewport_size();
    const bool horizontal_visible = scroll_axis_horizontal(state.axis()) &&
                                    content.w > metrics_viewport.w &&
                                    state.max_offset().x > 0.0f;
    const bool vertical_visible = scroll_axis_vertical(state.axis()) &&
                                  content.h > metrics_viewport.h &&
                                  state.max_offset().y > 0.0f;

    ScrollViewBars result;
    result.horizontal = make_horizontal_scrollbar(
        state, viewport, vertical_visible, thickness, min_thumb);
    result.vertical = make_vertical_scrollbar(
        state, viewport, horizontal_visible, thickness, min_thumb);
    return result;
}

} // namespace detail

[[nodiscard]] inline bool ensure_visible(
    ScrollState& state,
    Rect descendant_bounds,
    ScrollAlignment alignment = ScrollAlignment::Nearest) {
    const auto before = state.offset();
    auto target = before;
    const auto viewport = state.viewport_size();

    if (detail::scroll_axis_horizontal(state.axis())) {
        target.x = detail::ensure_visible_axis(
            before.x, viewport.w, descendant_bounds.x, descendant_bounds.w, alignment);
    }
    if (detail::scroll_axis_vertical(state.axis())) {
        target.y = detail::ensure_visible_axis(
            before.y, viewport.h, descendant_bounds.y, descendant_bounds.h, alignment);
    }

    state.set_offset(target);
    const auto after = state.offset();
    return after.x != before.x || after.y != before.y;
}

class ScrollViewComponent final : public Component {
public:
    ScrollViewComponent(ScrollState& state, bool pointer_pan)
        : state_(&state), pointer_pan_(pointer_pan) {}

    // The current retained tree routes ordinary pointer events to focusable
    // targets before bubbling. T034 needs the viewport itself to be targetable
    // when its content has no interactive descendant, so this is the smallest
    // GREEN for the existing RED wheel/pan contract. The final T034 review must
    // still separate pointer-targetability from keyboard focus if the overlay
    // architecture requires non-focusable scrollbar targets.
    [[nodiscard]] bool focusable() const noexcept override { return true; }

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

        switch (event.type) {
        case InputType::PointerDown:
            if (handle_scrollbar_down(event.position, context)) return EventResult::Handled;
            if (!pointer_pan_) return EventResult::Ignored;
            pan_active_ = true;
            pan_origin_ = event.position;
            offset_origin_ = state_->offset();
            context.capture_pointer();
            return EventResult::Handled;

        case InputType::PointerMove:
            if (scrollbar_drag_ != ScrollbarDrag::None) {
                update_scrollbar_drag(event.position);
                return EventResult::Handled;
            }
            if (!pan_active_) return EventResult::Ignored;
            state_->set_offset(Point{
                offset_origin_.x - (event.position.x - pan_origin_.x),
                offset_origin_.y - (event.position.y - pan_origin_.y)});
            return EventResult::Handled;

        case InputType::PointerUp:
        case InputType::PointerCancel:
            if (scrollbar_drag_ != ScrollbarDrag::None) {
                scrollbar_drag_ = ScrollbarDrag::None;
                context.release_pointer();
                return EventResult::Handled;
            }
            if (!pan_active_) return EventResult::Ignored;
            pan_active_ = false;
            context.release_pointer();
            return EventResult::Handled;

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override {
        pan_active_ = false;
        scrollbar_drag_ = ScrollbarDrag::None;
    }

    void paint(PaintContext&) const override {}

private:
    enum class ScrollbarDrag {
        None,
        Horizontal,
        Vertical,
    };

    [[nodiscard]] static bool same(Point a, Point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }

    [[nodiscard]] bool handle_scrollbar_down(Point position, InputContext& context) {
        const auto bars = detail::scroll_view_bars(*state_, context.bounds());

        if (bars.vertical.visible && bars.vertical.track.contains(position)) {
            if (bars.vertical.thumb.contains(position)) {
                begin_scrollbar_drag(
                    ScrollbarDrag::Vertical,
                    position.y,
                    state_->offset().y,
                    bars.vertical.track.h - bars.vertical.thumb.h,
                    state_->max_offset().y,
                    context);
            }
            return true;
        }

        if (bars.horizontal.visible && bars.horizontal.track.contains(position)) {
            if (bars.horizontal.thumb.contains(position)) {
                begin_scrollbar_drag(
                    ScrollbarDrag::Horizontal,
                    position.x,
                    state_->offset().x,
                    bars.horizontal.track.w - bars.horizontal.thumb.w,
                    state_->max_offset().x,
                    context);
            }
            return true;
        }

        return false;
    }

    void begin_scrollbar_drag(
        ScrollbarDrag drag,
        float pointer_position,
        float offset,
        float travel,
        float maximum,
        InputContext& context) {
        pan_active_ = false;
        scrollbar_drag_ = drag;
        scrollbar_pointer_origin_ = pointer_position;
        scrollbar_offset_origin_ = offset;
        scrollbar_travel_ = std::max(0.0f, travel);
        scrollbar_maximum_ = std::max(0.0f, maximum);
        context.capture_pointer();
    }

    void update_scrollbar_drag(Point position) {
        if (!(scrollbar_travel_ > 0.0f) || !(scrollbar_maximum_ > 0.0f)) return;
        const float pointer = scrollbar_drag_ == ScrollbarDrag::Horizontal ? position.x : position.y;
        if (!std::isfinite(pointer)) return;
        const double delta = static_cast<double>(pointer) - static_cast<double>(scrollbar_pointer_origin_);
        const double target = static_cast<double>(scrollbar_offset_origin_) +
                              delta / static_cast<double>(scrollbar_travel_) *
                                  static_cast<double>(scrollbar_maximum_);
        const float clamped = static_cast<float>(std::clamp(
            target, 0.0, static_cast<double>(scrollbar_maximum_)));
        const auto current = state_->offset();
        if (scrollbar_drag_ == ScrollbarDrag::Horizontal) {
            state_->set_offset(Point{clamped, current.y});
        } else {
            state_->set_offset(Point{current.x, clamped});
        }
    }

    ScrollState* state_{};
    bool pointer_pan_{};
    bool pan_active_{};
    Point pan_origin_{};
    Point offset_origin_{};
    ScrollbarDrag scrollbar_drag_{ScrollbarDrag::None};
    float scrollbar_pointer_origin_{};
    float scrollbar_offset_origin_{};
    float scrollbar_travel_{};
    float scrollbar_maximum_{};
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