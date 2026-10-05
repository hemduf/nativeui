#include <nativeui/scroll_view.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

bool detail::scroll_axis_horizontal(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both;
}

bool detail::scroll_axis_vertical(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Vertical || axis == ScrollAxis::Both;
}

float detail::ensure_visible_axis(
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

detail::ScrollbarAxisGeometry detail::make_horizontal_scrollbar(
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

detail::ScrollbarAxisGeometry detail::make_vertical_scrollbar(
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

detail::ScrollViewBars detail::scroll_view_bars_for_metrics(
    ScrollAxis axis,
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    float thickness ,
    float min_thumb ) noexcept {
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

detail::ScrollViewBars detail::scroll_view_bars(
    const ScrollState& state,
    Rect viewport,
    float thickness ,
    float min_thumb ) noexcept {
    return scroll_view_bars_for_metrics(
        state.axis(), state.offset(), state.viewport_size(), state.content_size(),
        viewport, thickness, min_thumb);
}

detail::ScrollbarAxisGeometry detail::scrollbar_for_track(
    const ScrollState& state,
    ScrollbarAxis axis,
    Rect track,
    float min_thumb ) noexcept {
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

bool ensure_visible(
    ScrollState& state,
    Rect descendant_bounds,
    ScrollAlignment alignment ) {
    const auto lifetime = state.lifetime_token();
    if (!lifetime.active()) return false;

    const auto before = state.offset();
    auto target = before;
    const auto viewport = state.viewport_size();
    const auto axis = state.axis();

    if (detail::scroll_axis_horizontal(axis)) {
        target.x = detail::ensure_visible_axis(
            before.x, viewport.w, descendant_bounds.x, descendant_bounds.w, alignment);
    }
    if (detail::scroll_axis_vertical(axis)) {
        target.y = detail::ensure_visible_axis(
            before.y, viewport.h, descendant_bounds.y, descendant_bounds.h, alignment);
    }

    state.set_offset(target);
    if (!lifetime.active()) return true;
    const auto after = state.offset();
    return after.x != before.x || after.y != before.y;
}

ScrollViewComponent::ScrollViewComponent(
    ScrollState* state,
    ScrollAxis axis,
    ScrollState::LifetimeToken lifetime,
    bool pointer_pan)
    : state_(state),
      axis_(axis),
      lifetime_(std::move(lifetime)),
      pointer_pan_(pointer_pan) {}

ScrollViewComponent::ScrollViewComponent(ScrollState& state, bool pointer_pan)
    : ScrollViewComponent(
          &state, state.axis(), state.lifetime_token(), pointer_pan) {}

bool ScrollViewComponent::focusable() const noexcept { return false; }

bool ScrollViewComponent::pointer_targetable() const noexcept { return true; }

Size ScrollViewComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

Size ScrollViewComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

Constraints ScrollViewComponent::child_constraints(
    const Constraints& constraints, std::size_t index, std::size_t) const {
    if (index != 0) return constraints.loosen();
    auto child = constraints.loosen();
    if (detail::scroll_axis_horizontal(axis_)) child.max.w = kUnboundedExtent;
    if (detail::scroll_axis_vertical(axis_)) child.max.h = kUnboundedExtent;
    return child;
}

void ScrollViewComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    if (placements.empty()) return;

    // Child 0 is the retained scroll component. Measure it naturally here
    // so scrollbar overlays can be placed from the same content dimensions.
    Size content = children.empty() ? Size{} : children.front().preferred;
    if (axis_ == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
    if (axis_ == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);

    placements[0].bounds = bounds;
    const bool horizontal_visible = detail::scroll_axis_horizontal(axis_) &&
                                    content.w > bounds.w;
    const bool vertical_visible = detail::scroll_axis_vertical(axis_) &&
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

EventResult ScrollViewComponent::input(const InputEvent& event, InputContext& context) {
    if (!alive()) {
        if (pan_active_ &&
            (event.type == InputType::PointerMove ||
             event.type == InputType::PointerUp ||
             event.type == InputType::PointerCancel)) {
            pan_active_ = false;
            context.release_pointer();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }

    if (event.type == InputType::PointerWheel) {
        const auto before = state_->offset();
        state_->scroll_by(event.delta);
        if (!alive()) return EventResult::Handled;
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
        if (!alive()) {
            pan_active_ = false;
            context.release_pointer();
        }
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

void ScrollViewComponent::descendant_focus_changed(Rect descendant_bounds) {
    if (!alive()) return;
    const auto offset = state_->offset();
    descendant_bounds.x += offset.x;
    descendant_bounds.y += offset.y;
    (void)ensure_visible(*state_, descendant_bounds, ScrollAlignment::Nearest);
}

void ScrollViewComponent::deactivate(LifecycleContext&) { pan_active_ = false; }

void ScrollViewComponent::paint(PaintContext&) const {}

bool ScrollViewComponent::alive() const noexcept {
    return state_ != nullptr && lifetime_.active();
}

bool ScrollViewComponent::same(Point a, Point b) noexcept {
    return a.x == b.x && a.y == b.y;
}

float ScrollViewComponent::normalized_thickness(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

detail::ScrollbarComponent::ScrollbarComponent(
    ScrollState* state,
    ScrollState::LifetimeToken lifetime,
    ScrollbarAxis axis,
    ScrollbarStyle style)
    : state_(state),
      lifetime_(std::move(lifetime)),
      axis_(axis),
      style_(std::move(style)) {}

detail::ScrollbarComponent::ScrollbarComponent(ScrollState& state, ScrollbarAxis axis, ScrollbarStyle style)
    : ScrollbarComponent(&state, state.lifetime_token(), axis, std::move(style)) {}

bool detail::ScrollbarComponent::pointer_targetable() const noexcept { return true; }

Size detail::ScrollbarComponent::measure(const std::vector<ChildMetrics>&) const {
    const float thickness = sanitized_thickness(resolved_style().thickness);
    return Size{thickness, thickness};
}

EventResult detail::ScrollbarComponent::input(const InputEvent& event, InputContext& context) {
    if (!alive()) {
        if (drag_active_ &&
            (event.type == InputType::PointerMove ||
             event.type == InputType::PointerUp ||
             event.type == InputType::PointerCancel ||
             event.type == InputType::PointerLeave)) {
            drag_active_ = false;
            hovered_ = false;
            context.release_pointer();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }

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
        if (was_dragging) {
            update_drag(event.position);
            if (!alive()) {
                drag_active_ = false;
                hovered_ = false;
                context.release_pointer();
                return EventResult::Handled;
            }
        }
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

void detail::ScrollbarComponent::deactivate(LifecycleContext& context) {
    if (!drag_active_ && !hovered_) return;
    const auto before = resolved_style();
    drag_active_ = false;
    hovered_ = false;
    (void)invalidate_style_transition(context, before);
}

void detail::ScrollbarComponent::paint(PaintContext& context) const {
    if (!alive()) return;
    const auto resolved = resolved_style();
    const auto geometry = scrollbar_for_track(
        *state_, axis_, context.bounds(), resolved.minimum_thumb);
    if (!geometry.visible) return;
    const float radius = sanitized_nonnegative(resolved.corner_radius);
    auto& painter = context.painter();
    painter.fill_rounded_rect(geometry.track, radius, resolved.track);
    painter.fill_rounded_rect(geometry.thumb, radius, resolved.thumb);
}

bool detail::ScrollbarComponent::alive() const noexcept {
    return state_ != nullptr && lifetime_.active();
}

VisualState detail::ScrollbarComponent::current_visual_state() const noexcept {
    return VisualState{
        .enabled = effective_enabled(),
        .read_only = effective_read_only(),
        .hovered = hovered_,
        .pressed = drag_active_,
    };
}

ResolvedScrollbarStyle detail::ScrollbarComponent::resolved_style() const {
    return resolve_scrollbar_style(
        default_scrollbar_style(current_theme()), style_, current_visual_state());
}

float detail::ScrollbarComponent::sanitized_nonnegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

float detail::ScrollbarComponent::sanitized_thickness(float value) noexcept {
    return sanitized_nonnegative(value);
}

void detail::ScrollbarComponent::update_drag(Point position) {
    if (!alive() || !(travel_ > 0.0f) || !(maximum_ > 0.0f)) return;
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

ScrollView&& ScrollView::pointer_pan(bool enabled ) && {
    pointer_pan_ = enabled;
    return std::move(*this);
}

ScrollView&& ScrollView::style(ScrollbarStyle value) && {
    style_ = std::move(value);
    return std::move(*this);
}

Spec ScrollView::spec() && {
    auto* state = state_;
    const auto axis = axis_;
    const auto lifetime = lifetime_;

    std::vector<Spec> scroll_children;
    scroll_children.push_back(std::move(content_));
    Spec scroll{
        [state, axis, lifetime] {
            return std::make_unique<detail::RetainedScrollComponent>(
                state, axis, lifetime);
        },
        std::move(scroll_children)};

    auto style = std::move(style_);
    std::vector<Spec> children;
    // Paint order is content first, then horizontal and vertical overlays.
    // Pointer hit testing walks children in reverse paint order, so occupied
    // scrollbar tracks win over interactive content without a global router.
    children.push_back(std::move(scroll));
    children.push_back(Spec{
        [state, lifetime, style] {
            return std::make_unique<detail::ScrollbarComponent>(
                state, lifetime, detail::ScrollbarAxis::Horizontal, style);
        },
        {}});
    children.push_back(Spec{
        [state, lifetime, style] {
            return std::make_unique<detail::ScrollbarComponent>(
                state, lifetime, detail::ScrollbarAxis::Vertical, style);
        },
        {}});

    const bool pointer_pan = pointer_pan_;
    return Spec{
        [state, axis, lifetime, pointer_pan] {
            return std::make_unique<ScrollViewComponent>(
                state, axis, lifetime, pointer_pan);
        },
        std::move(children)};
}

} // namespace ui
