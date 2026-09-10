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
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    bool vertical_visible,
    float thickness,
    float min_thumb) noexcept {
    ScrollbarAxisGeometry result;
    const float maximum = std::max(0.0f, content.w - metrics_viewport.w);
    if (!(content.w > metrics_viewport.w) || !(maximum > 0.0f)) return result;

    const float track_length = std::max(0.0f, viewport.w - (vertical_visible ? thickness : 0.0f));
    if (!(track_length > 0.0f)) return result;

    result.visible = true;
    result.track = Rect{viewport.x, viewport.y + std::max(0.0f, viewport.h - thickness),
                        track_length, thickness};
    const float requested = track_length * (metrics_viewport.w / content.w);
    const float thumb_length = std::clamp(std::max(min_thumb, requested), 0.0f, track_length);
    const float travel = std::max(0.0f, track_length - thumb_length);
    const float fraction = std::clamp(offset.x / maximum, 0.0f, 1.0f);
    result.thumb = Rect{result.track.x + travel * fraction, result.track.y,
                        thumb_length, thickness};
    return result;
}

[[nodiscard]] inline ScrollbarAxisGeometry make_vertical_scrollbar(
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    bool horizontal_visible,
    float thickness,
    float min_thumb) noexcept {
    ScrollbarAxisGeometry result;
    const float maximum = std::max(0.0f, content.h - metrics_viewport.h);
    if (!(content.h > metrics_viewport.h) || !(maximum > 0.0f)) return result;

    const float track_length = std::max(0.0f, viewport.h - (horizontal_visible ? thickness : 0.0f));
    if (!(track_length > 0.0f)) return result;

    result.visible = true;
    result.track = Rect{viewport.x + std::max(0.0f, viewport.w - thickness), viewport.y,
                        thickness, track_length};
    const float requested = track_length * (metrics_viewport.h / content.h);
    const float thumb_length = std::clamp(std::max(min_thumb, requested), 0.0f, track_length);
    const float travel = std::max(0.0f, track_length - thumb_length);
    const float fraction = std::clamp(offset.y / maximum, 0.0f, 1.0f);
    result.thumb = Rect{result.track.x, result.track.y + travel * fraction,
                        thickness, thumb_length};
    return result;
}

[[nodiscard]] inline ScrollViewBars scroll_view_bars_for_metrics(
    ScrollAxis axis,
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    float thickness = 8.0f,
    float min_thumb = 18.0f) noexcept {
    thickness = std::isfinite(thickness) ? std::max(0.0f, thickness) : 0.0f;
    min_thumb = std::isfinite(min_thumb) ? std::max(0.0f, min_thumb) : 0.0f;

    metrics_viewport = Size{
        std::isfinite(metrics_viewport.w) ? std::max(0.0f, metrics_viewport.w) : 0.0f,
        std::isfinite(metrics_viewport.h) ? std::max(0.0f, metrics_viewport.h) : 0.0f};
    content = Size{
        std::isfinite(content.w) ? std::max(0.0f, content.w) : 0.0f,
        std::isfinite(content.h) ? std::max(0.0f, content.h) : 0.0f};

    const bool horizontal_visible = scroll_axis_horizontal(axis) &&
                                    content.w > metrics_viewport.w;
    const bool vertical_visible = scroll_axis_vertical(axis) &&
                                  content.h > metrics_viewport.h;

    const Point maximum{
        horizontal_visible ? std::max(0.0f, content.w - metrics_viewport.w) : 0.0f,
        vertical_visible ? std::max(0.0f, content.h - metrics_viewport.h) : 0.0f};
    offset.x = std::clamp(std::isfinite(offset.x) ? offset.x : 0.0f, 0.0f, maximum.x);
    offset.y = std::clamp(std::isfinite(offset.y) ? offset.y : 0.0f, 0.0f, maximum.y);

    ScrollViewBars result;
    if (horizontal_visible) {
        result.horizontal = make_horizontal_scrollbar(
            offset, metrics_viewport, content, viewport, vertical_visible, thickness, min_thumb);
    }
    if (vertical_visible) {
        result.vertical = make_vertical_scrollbar(
            offset, metrics_viewport, content, viewport, horizontal_visible, thickness, min_thumb);
    }
    return result;
}

[[nodiscard]] inline ScrollViewBars scroll_view_bars(
    const ScrollState& state,
    Rect viewport,
    float thickness = 8.0f,
    float min_thumb = 18.0f) noexcept {
    return scroll_view_bars_for_metrics(
        state.axis(), state.offset(), state.viewport_size(), state.content_size(),
        viewport, thickness, min_thumb);
}

enum class ScrollbarAxis {
    Horizontal,
    Vertical,
};

[[nodiscard]] inline ScrollbarAxisGeometry scrollbar_for_track(
    const ScrollState& state,
    ScrollbarAxis axis,
    Rect track,
    float min_thumb = 18.0f) noexcept {
    ScrollbarAxisGeometry result;
    const auto content = state.content_size();
    const auto viewport = state.viewport_size();
    const auto maximum = state.max_offset();
    min_thumb = std::isfinite(min_thumb) ? std::max(0.0f, min_thumb) : 0.0f;

    const bool horizontal = axis == ScrollbarAxis::Horizontal;
    const float content_extent = horizontal ? content.w : content.h;
    const float viewport_extent = horizontal ? viewport.w : viewport.h;
    const float maximum_offset = horizontal ? maximum.x : maximum.y;
    const float track_extent = horizontal ? track.w : track.h;
    if (!(content_extent > viewport_extent) || !(maximum_offset > 0.0f) ||
        !(track_extent > 0.0f)) {
        return result;
    }

    const float requested = track_extent * (viewport_extent / content_extent);
    const float thumb_extent = std::clamp(std::max(min_thumb, requested), 0.0f, track_extent);
    const float travel = std::max(0.0f, track_extent - thumb_extent);
    const float offset = horizontal ? state.offset().x : state.offset().y;
    const float fraction = std::clamp(offset / maximum_offset, 0.0f, 1.0f);

    result.visible = true;
    result.track = track;
    result.thumb = horizontal
        ? Rect{track.x + travel * fraction, track.y, thumb_extent, track.h}
        : Rect{track.x, track.y + travel * fraction, track.w, thumb_extent};
    return result;
}

class ScrollbarComponent final : public Component {
public:
    ScrollbarComponent(ScrollState& state, ScrollbarAxis axis)
        : state_(&state), axis_(axis) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override { return {}; }

    EventResult input(const InputEvent& event, InputContext& context) override {
        switch (event.type) {
        case InputType::PointerDown: {
            const auto geometry = scrollbar_for_track(*state_, axis_, context.bounds());
            if (!geometry.visible) return EventResult::Ignored;
            if (!geometry.thumb.contains(event.position)) {
                return EventResult::Handled;
            }

            drag_active_ = true;
            pointer_origin_ = axis_ == ScrollbarAxis::Horizontal
                ? event.position.x
                : event.position.y;
            const auto offset = state_->offset();
            offset_origin_ = axis_ == ScrollbarAxis::Horizontal ? offset.x : offset.y;
            const auto maximum = state_->max_offset();
            maximum_ = axis_ == ScrollbarAxis::Horizontal ? maximum.x : maximum.y;
            const float track_extent = axis_ == ScrollbarAxis::Horizontal
                ? geometry.track.w
                : geometry.track.h;
            const float thumb_extent = axis_ == ScrollbarAxis::Horizontal
                ? geometry.thumb.w
                : geometry.thumb.h;
            travel_ = std::max(0.0f, track_extent - thumb_extent);
            context.capture_pointer();
            return EventResult::Handled;
        }

        case InputType::PointerMove:
            if (!drag_active_) return EventResult::Ignored;
            update_drag(event.position);
            return EventResult::Handled;

        case InputType::PointerUp:
        case InputType::PointerCancel:
            if (!drag_active_) return EventResult::Ignored;
            drag_active_ = false;
            context.release_pointer();
            return EventResult::Handled;

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override { drag_active_ = false; }

    void paint(PaintContext& context) const override {
        const auto geometry = scrollbar_for_track(*state_, axis_, context.bounds());
        if (!geometry.visible) return;
        auto& painter = context.painter();
        painter.fill_rounded_rect(geometry.track, 4.0f, colors::input);
        painter.fill_rounded_rect(geometry.thumb, 4.0f, colors::borderFocus);
    }

private:
    void update_drag(Point position) {
        if (!(travel_ > 0.0f) || !(maximum_ > 0.0f)) return;
        const float pointer = axis_ == ScrollbarAxis::Horizontal ? position.x : position.y;
        if (!std::isfinite(pointer)) return;
        const double delta = static_cast<double>(pointer) - static_cast<double>(pointer_origin_);
        const double target = static_cast<double>(offset_origin_) +
                              delta / static_cast<double>(travel_) *
                                  static_cast<double>(maximum_);
        const float clamped = static_cast<float>(std::clamp(
            target, 0.0, static_cast<double>(maximum_)));
        const auto current = state_->offset();
        if (axis_ == ScrollbarAxis::Horizontal) {
            state_->set_offset(Point{clamped, current.y});
        } else {
            state_->set_offset(Point{current.x, clamped});
        }
    }

    ScrollState* state_{};
    ScrollbarAxis axis_{ScrollbarAxis::Vertical};
    bool drag_active_{};
    float pointer_origin_{};
    float offset_origin_{};
    float travel_{};
    float maximum_{};
};

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

    // Pointer targetability is independent from keyboard focus in the retained
    // tree. ScrollView stays out of Tab traversal while receiving bubbled wheel
    // and optional pan events when descendants do not consume them.
    [[nodiscard]] bool focusable() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t) const override {
        if (index != 0) return Constraints::tight({});
        auto child = constraints.loosen();
        if (detail::scroll_axis_horizontal(state_->axis())) child.max.w = kUnboundedExtent;
        if (detail::scroll_axis_vertical(state_->axis())) child.max.h = kUnboundedExtent;
        return child;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (placements.empty()) return;

        // Child 0 is the existing T012 Scroll component. Measure it naturally
        // here so scrollbar overlays can be placed from the same content
        // dimensions before the child updates ScrollState during its own layout.
        Size content = children.empty() ? Size{} : children.front().preferred;
        if (state_->axis() == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
        if (state_->axis() == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);

        placements[0].bounds = bounds;
        const auto bars = detail::scroll_view_bars_for_metrics(
            state_->axis(), state_->offset(), Size{bounds.w, bounds.h}, content, bounds);

        if (placements.size() > 1) {
            placements[1].bounds = bars.horizontal.visible
                ? bars.horizontal.track
                : Rect{bounds.x, bounds.y, 0.0f, 0.0f};
        }
        if (placements.size() > 2) {
            placements[2].bounds = bars.vertical.visible
                ? bars.vertical.track
                : Rect{bounds.x, bounds.y, 0.0f, 0.0f};
        }
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (event.type == InputType::PointerWheel) {
            const auto before = state_->offset();
            state_->scroll_by(event.delta);
            return same(before, state_->offset()) ? EventResult::Ignored : EventResult::Handled;
        }

        switch (event.type) {
        case InputType::PointerDown:
            if (!pointer_pan_) return EventResult::Ignored;
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
        // Paint order is content first, then horizontal and vertical overlays.
        // Pointer hit testing walks children in reverse paint order, so occupied
        // scrollbar tracks win over interactive content without a global router.
        children.push_back(std::move(scroll));
        children.push_back(Spec{
            [state] {
                return std::make_unique<detail::ScrollbarComponent>(
                    *state, detail::ScrollbarAxis::Horizontal);
            },
            {}});
        children.push_back(Spec{
            [state] {
                return std::make_unique<detail::ScrollbarComponent>(
                    *state, detail::ScrollbarAxis::Vertical);
            },
            {}});

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
