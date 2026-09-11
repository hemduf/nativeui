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

class ScrollbarComponent final : public Component, public ThemeBinding {
public:
    ScrollbarComponent(ScrollState& state, ScrollbarAxis axis, ScrollbarStyle style)
        : state_(&state), axis_(axis), style_(std::move(style)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const float thickness = sanitized_thickness(resolved_style().thickness);
        return Size{thickness, thickness};
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        switch (event.type) {
        case InputType::PointerDown: {
            const auto before = resolved_style();
            const auto geometry = scrollbar_for_track(
                *state_, axis_, context.bounds(), before.minimum_thumb);
            if (!geometry.visible) return EventResult::Ignored;

            hovered_ = true;
            if (!geometry.thumb.contains(event.position)) {
                (void)invalidate_style_transition(context, before);
                return EventResult::Handled;
            }

            drag_active_ = true;
            const auto pressed = resolved_style();
            const auto pressed_geometry = scrollbar_for_track(
                *state_, axis_, context.bounds(), pressed.minimum_thumb);
            pointer_origin_ = axis_ == ScrollbarAxis::Horizontal
                ? event.position.x
                : event.position.y;
            const auto offset = state_->offset();
            offset_origin_ = axis_ == ScrollbarAxis::Horizontal ? offset.x : offset.y;
            const auto maximum = state_->max_offset();
            maximum_ = axis_ == ScrollbarAxis::Horizontal ? maximum.x : maximum.y;
            const float track_extent = axis_ == ScrollbarAxis::Horizontal
                ? pressed_geometry.track.w
                : pressed_geometry.track.h;
            const float thumb_extent = axis_ == ScrollbarAxis::Horizontal
                ? pressed_geometry.thumb.w
                : pressed_geometry.thumb.h;
            travel_ = std::max(0.0f, track_extent - thumb_extent);
            context.capture_pointer();
            (void)invalidate_style_transition(context, before);
            return EventResult::Handled;
        }

        case InputType::PointerMove: {
            const auto before = resolved_style();
            const bool was_dragging = drag_active_;
            hovered_ = context.bounds().contains(event.position);
            if (was_dragging) update_drag(event.position);
            const bool layout_changed = invalidate_style_transition(context, before);
            return was_dragging || layout_changed
                ? EventResult::Handled
                : EventResult::Ignored;
        }

        case InputType::PointerLeave: {
            const auto before = resolved_style();
            hovered_ = false;
            return invalidate_style_transition(context, before)
                ? EventResult::Handled
                : EventResult::Ignored;
        }

        case InputType::PointerUp: {
            if (!drag_active_) return EventResult::Ignored;
            const auto before = resolved_style();
            drag_active_ = false;
            hovered_ = context.bounds().contains(event.position);
            context.release_pointer();
            (void)invalidate_style_transition(context, before);
            return EventResult::Handled;
        }

        case InputType::PointerCancel: {
            if (!drag_active_) return EventResult::Ignored;
            const auto before = resolved_style();
            drag_active_ = false;
            hovered_ = false;
            context.release_pointer();
            (void)invalidate_style_transition(context, before);
            return EventResult::Handled;
        }

        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext& context) override {
        if (!drag_active_ && !hovered_) return;
        const auto before = resolved_style();
        drag_active_ = false;
        hovered_ = false;
        (void)invalidate_style_transition(context, before);
    }

    void paint(PaintContext& context) const override {
        const auto resolved = resolved_style();
        const auto geometry = scrollbar_for_track(
            *state_, axis_, context.bounds(), resolved.minimum_thumb);
        if (!geometry.visible) return;
        const float radius = sanitized_nonnegative(resolved.corner_radius);
        auto& painter = context.painter();
        painter.fill_rounded_rect(geometry.track, radius, resolved.track);
        painter.fill_rounded_rect(geometry.thumb, radius, resolved.thumb);
    }

private:
    [[nodiscard]] VisualState current_visual_state() const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = hovered_,
            .pressed = drag_active_,
        };
    }

    [[nodiscard]] ResolvedScrollbarStyle resolved_style() const {
        return resolve_scrollbar_style(
            default_scrollbar_style(current_theme()), style_, current_visual_state());
    }

    [[nodiscard]] static float sanitized_nonnegative(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    [[nodiscard]] static float sanitized_thickness(float value) noexcept {
        return sanitized_nonnegative(value);
    }

    template <class Context>
    [[nodiscard]] bool invalidate_style_transition(
        Context& context,
        const ResolvedScrollbarStyle& before) const {
        const auto after = resolved_style();
        if (after == before) return false;
        if (sanitized_thickness(after.thickness) !=
            sanitized_thickness(before.thickness)) {
            context.invalidate_layout();
            return true;
        }
        context.invalidate();
        return false;
    }

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
    ScrollbarStyle style_;
    bool drag_active_{};
    bool hovered_{};
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
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t) const override {
        if (index != 0) return constraints.loosen();
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
        const bool horizontal_visible = detail::scroll_axis_horizontal(state_->axis()) &&
                                        content.w > bounds.w;
        const bool vertical_visible = detail::scroll_axis_vertical(state_->axis()) &&
                                      content.h > bounds.h;
        const float horizontal_thickness = horizontal_visible && children.size() > 1
            ? normalized_thickness(children[1].preferred.h)
            : 0.0f;
        const float vertical_thickness = vertical_visible && children.size() > 2
            ? normalized_thickness(children[2].preferred.w)
            : 0.0f;

        if (placements.size() > 1) {
            placements[1].bounds = horizontal_visible
                ? Rect{
                      bounds.x,
                      bounds.y + std::max(0.0f, bounds.h - horizontal_thickness),
                      std::max(0.0f, bounds.w - vertical_thickness),
                      horizontal_thickness}
                : Rect{bounds.x, bounds.y, 0.0f, 0.0f};
        }
        if (placements.size() > 2) {
            placements[2].bounds = vertical_visible
                ? Rect{
                      bounds.x + std::max(0.0f, bounds.w - vertical_thickness),
                      bounds.y,
                      vertical_thickness,
                      std::max(0.0f, bounds.h - horizontal_thickness)}
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

    void descendant_focus_changed(Rect descendant_bounds) override {
        const auto offset = state_->offset();
        descendant_bounds.x += offset.x;
        descendant_bounds.y += offset.y;
        (void)ensure_visible(*state_, descendant_bounds, ScrollAlignment::Nearest);
    }

    void deactivate(LifecycleContext&) override { pan_active_ = false; }

    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] static bool same(Point a, Point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }

    [[nodiscard]] static float normalized_thickness(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
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

    ScrollView&& style(ScrollbarStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;

        std::vector<Spec> scroll_children;
        scroll_children.push_back(std::move(content_));
        Spec scroll{
            [state] { return std::make_unique<ScrollComponent>(*state); },
            std::move(scroll_children)};

        auto style = std::move(style_);
        std::vector<Spec> children;
        // Paint order is content first, then horizontal and vertical overlays.
        // Pointer hit testing walks children in reverse paint order, so occupied
        // scrollbar tracks win over interactive content without a global router.
        children.push_back(std::move(scroll));
        children.push_back(Spec{
            [state, style] {
                return std::make_unique<detail::ScrollbarComponent>(
                    *state, detail::ScrollbarAxis::Horizontal, style);
            },
            {}});
        children.push_back(Spec{
            [state, style] {
                return std::make_unique<detail::ScrollbarComponent>(
                    *state, detail::ScrollbarAxis::Vertical, style);
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
    ScrollbarStyle style_;
};