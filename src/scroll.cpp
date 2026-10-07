#include <nativeui/scroll.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

ScrollState::Control::Control(ScrollAxis value) : axis(value) {}

bool ScrollState::Control::has_listener(std::size_t id) const noexcept {
    for (const auto& listener : listeners) {
        if (listener && listener->id == id && listener->active) return true;
    }
    return false;
}

void ScrollState::Control::remove_listener(std::size_t id) noexcept {
    for (auto& listener : listeners) {
        if (listener && listener->id == id) {
            listener->active = false;
            cleanup_needed = true;
            break;
        }
    }
    if (!dispatching && cleanup_needed) compact_inactive();
}

void ScrollState::Control::invalidate_owner() noexcept {
    owner_alive = false;
    pending_offset.reset();
    for (auto& listener : listeners) {
        if (listener) listener->active = false;
    }
    cleanup_needed = true;
    if (!dispatching) compact_inactive();
}

void ScrollState::Control::compact_inactive() noexcept {
    std::erase_if(listeners, [](const auto& listener) {
        return !listener || !listener->active;
    });
    cleanup_needed = false;
}

bool ScrollState::LifetimeToken::active() const noexcept {
    if (auto control = control_.lock()) return control->owner_alive;
    return false;
}

ScrollState::LifetimeToken::LifetimeToken(std::weak_ptr<Control> control)
    : control_(std::move(control)) {}

ScrollState::Subscription::Subscription(std::weak_ptr<Control> control, std::size_t id)
    : control_(std::move(control)), id_(id) {}

ScrollState::Subscription::Subscription(Subscription&& other) noexcept
    : control_(std::move(other.control_)), id_(std::exchange(other.id_, 0)) {}

ScrollState::Subscription& ScrollState::Subscription::operator=(Subscription&& other) noexcept {
    if (this == &other) return *this;
    reset();
    control_ = std::move(other.control_);
    id_ = std::exchange(other.id_, 0);
    return *this;
}

ScrollState::Subscription::~Subscription() { reset(); }

void ScrollState::Subscription::reset() noexcept {
    if (id_ == 0) return;
    if (auto control = control_.lock()) control->remove_listener(id_);
    control_.reset();
    id_ = 0;
}

bool ScrollState::Subscription::active() const noexcept {
    if (id_ == 0) return false;
    if (auto control = control_.lock()) {
        return control->owner_alive && control->has_listener(id_);
    }
    return false;
}

ScrollState::ScrollState(ScrollAxis axis )
    : control_(std::make_shared<Control>(axis)) {}

ScrollState::~ScrollState() { control_->invalidate_owner(); }

ScrollAxis ScrollState::axis() const noexcept { return control_->axis; }

Point ScrollState::offset() const noexcept { return control_->offset; }

Size ScrollState::viewport_size() const noexcept { return control_->viewport; }

Size ScrollState::content_size() const noexcept { return control_->content; }

Point ScrollState::max_offset() const noexcept { return maximum_offset(*control_); }

ScrollState::LifetimeToken ScrollState::lifetime_token() const noexcept {
    return LifetimeToken{control_};
}

void ScrollState::set_offset(Point value) {
    auto control = control_;
    dispatch_offset(control, clamp(*control, value));
}

void ScrollState::scroll_by(Point delta) {
    auto control = control_;
    if (!control->owner_alive) return;
    const Point base = control->dispatching && control->pending_offset
                           ? *control->pending_offset
                           : control->offset;
    dispatch_offset(
        control,
        Point{base.x + finite_or_zero(delta.x), base.y + finite_or_zero(delta.y)});
}

ScrollState::Subscription ScrollState::observe(std::function<void(Point)> callback) {
    auto control = control_;
    if (!control->owner_alive) return {};
    const auto id = control->next_listener_id++;
    control->listeners.push_back(
        std::make_unique<Listener>(Listener{id, true, std::move(callback)}));
    return Subscription{control, id};
}

bool ScrollState::allows_horizontal(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both;
}

bool ScrollState::allows_vertical(ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Vertical || axis == ScrollAxis::Both;
}

float ScrollState::finite_or_zero(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}

bool ScrollState::same(Point a, Point b) noexcept {
    return a.x == b.x && a.y == b.y;
}

Point ScrollState::maximum_offset(const Control& control) noexcept {
    return Point{
        allows_horizontal(control.axis)
            ? std::max(0.0f, control.content.w - control.viewport.w)
            : 0.0f,
        allows_vertical(control.axis)
            ? std::max(0.0f, control.content.h - control.viewport.h)
            : 0.0f};
}

Point ScrollState::clamp(const Control& control, Point value) noexcept {
    value.x = allows_horizontal(control.axis)
                  ? std::max(0.0f, finite_or_zero(value.x))
                  : 0.0f;
    value.y = allows_vertical(control.axis)
                  ? std::max(0.0f, finite_or_zero(value.y))
                  : 0.0f;
    if (control.metrics_valid) {
        const auto maximum = maximum_offset(control);
        value.x = std::min(value.x, maximum.x);
        value.y = std::min(value.y, maximum.y);
    }
    return value;
}

void ScrollState::dispatch_offset(const std::shared_ptr<Control>& control, Point value) {
    if (!control || !control->owner_alive) return;
    value = clamp(*control, value);

    if (control->dispatching) {
        if (same(value, control->offset)) {
            control->pending_offset.reset();
        } else {
            control->pending_offset = value;
        }
        return;
    }
    if (same(value, control->offset)) return;

    control->pending_offset = value;
    control->dispatching = true;
    try {
        while (control->owner_alive && control->pending_offset) {
            Point next = clamp(*control, *control->pending_offset);
            control->pending_offset.reset();
            if (same(next, control->offset)) continue;

            control->offset = next;
            const Point pass_value = control->offset;
            const std::size_t pass_size = control->listeners.size();
            for (std::size_t index = 0;
                 index < pass_size && control->owner_alive;
                 ++index) {
                Listener* listener = control->listeners[index].get();
                if (!listener || !listener->active || !listener->callback) continue;
                listener->callback(pass_value);
            }
        }
    } catch (...) {
        // Match State<T>'s v1 failure transaction: the current pass value
        // remains committed, but recursive pending work and the unstarted
        // suffix are not retried implicitly after an observer throws.
        control->pending_offset.reset();
        control->dispatching = false;
        if (control->cleanup_needed) control->compact_inactive();
        throw;
    }

    control->dispatching = false;
    if (control->cleanup_needed) control->compact_inactive();
}

void ScrollState::update_metrics(Size viewport, Size content) {
    auto control = control_;
    if (!control->owner_alive) return;
    control->viewport = Size{std::max(0.0f, finite_or_zero(viewport.w)),
                             std::max(0.0f, finite_or_zero(viewport.h))};
    control->content = Size{std::max(0.0f, finite_or_zero(content.w)),
                            std::max(0.0f, finite_or_zero(content.h))};
    control->metrics_valid = true;
    dispatch_offset(control, clamp(*control, control->offset));
}

bool detail::ScrollMetricsAccess::publish(ScrollState* state,ScrollState::LifetimeToken lifetime,
    Size viewport,Size content) {
    if (!state || !lifetime.active()) return false;
    state->update_metrics(viewport,content);
    return lifetime.active();
}

ScrollComponent::ScrollComponent(ScrollState& state)
    : state_(&state), axis_(state.axis()), lifetime_(state.lifetime_token()) {}

bool ScrollComponent::clips_children() const noexcept { return true; }

Size ScrollComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

Size ScrollComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    if (children.empty()) return {};
    auto minimum = children.front().minimum;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
        minimum.w = 0.0f;
    }
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
        minimum.h = 0.0f;
    }
    return minimum;
}

Constraints ScrollComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    Size maximum = constraints.max;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
        maximum.w = kUnboundedExtent;
    }
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
        maximum.h = kUnboundedExtent;
    }
    return Constraints::loose(maximum);
}

void ScrollComponent::mount(MountContext& context) {
    if (!alive()) return;
    subscription_ = state_->observe(
        [invalidate = context.layout_invalidator()](Point) { invalidate(); });
}

void ScrollComponent::unmount(LifecycleContext&) { subscription_.reset(); }

void ScrollComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    if (placements.empty() || children.empty()) {
        if (alive()) state_->update_metrics({bounds.w, bounds.h}, {});
        return;
    }

    Size content = children.front().preferred;
    if (axis_ == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
    if (axis_ == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);

    if (alive()) {
        state_->update_metrics({bounds.w, bounds.h}, content);
        if (alive()) {
            const auto offset = state_->offset();
            placements.front().bounds = Rect{
                bounds.x - offset.x, bounds.y - offset.y, content.w, content.h};
            return;
        }
    }

    placements.front().bounds = Rect{bounds.x, bounds.y, content.w, content.h};
}

void ScrollComponent::paint(PaintContext&) const {}

bool ScrollComponent::alive() const noexcept {
    return state_ != nullptr && lifetime_.active();
}

detail::RetainedScrollComponent::RetainedScrollComponent(
    ScrollState* state,
    ScrollAxis axis,
    ScrollState::LifetimeToken lifetime)
    : state_(state), axis_(axis), lifetime_(std::move(lifetime)) {}

bool detail::RetainedScrollComponent::clips_children() const noexcept { return true; }

Size detail::RetainedScrollComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

Size detail::RetainedScrollComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    if (children.empty()) return {};
    auto minimum = children.front().minimum;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) minimum.w = 0.0f;
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) minimum.h = 0.0f;
    return minimum;
}

Constraints detail::RetainedScrollComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    Size maximum = constraints.max;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
        maximum.w = kUnboundedExtent;
    }
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
        maximum.h = kUnboundedExtent;
    }
    return Constraints::loose(maximum);
}

void detail::RetainedScrollComponent::mount(MountContext& context) {
    if (!alive()) return;
    subscription_ = state_->observe(
        [invalidate = context.layout_invalidator()](Point) { invalidate(); });
}

void detail::RetainedScrollComponent::unmount(LifecycleContext&) { subscription_.reset(); }

void detail::RetainedScrollComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    if (placements.empty() || children.empty()) {
        if (alive()) state_->update_metrics({bounds.w, bounds.h}, {});
        return;
    }

    Size content = children.front().preferred;
    if (axis_ == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
    if (axis_ == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);

    if (alive()) {
        state_->update_metrics({bounds.w, bounds.h}, content);
        if (alive()) {
            const auto offset = state_->offset();
            placements.front().bounds = Rect{
                bounds.x - offset.x, bounds.y - offset.y, content.w, content.h};
            return;
        }
    }

    placements.front().bounds = Rect{bounds.x, bounds.y, content.w, content.h};
}

void detail::RetainedScrollComponent::paint(PaintContext&) const {}

bool detail::RetainedScrollComponent::alive() const noexcept {
    return state_ != nullptr && lifetime_.active();
}

Spec Scroll::spec() && {
    auto* state = state_;
    const auto axis = axis_;
    const auto lifetime = lifetime_;
    return Spec{
        [state, axis, lifetime] {
            return std::make_unique<detail::RetainedScrollComponent>(
                state, axis, lifetime);
        },
        std::move(children_)};
}

} // namespace ui
