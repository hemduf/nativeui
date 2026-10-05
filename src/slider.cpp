#include <nativeui/slider.hpp>

namespace ui {

namespace detail {
float SliderTrackAxis::position(float fraction) const noexcept {
  if (!std::isfinite(fraction))
    fraction = 0.0f;
  const float normalized = std::clamp(fraction, 0.0f, 1.0f);
  const float span = end - start;
  return inverted ? end - span * normalized : start + span * normalized;
}

float SliderTrackAxis::fraction(float coordinate) const noexcept {
  if (!std::isfinite(coordinate))
    return 0.0f;
  const float span = end - start;
  if (!(span > 0.0f))
    return 0.0f;
  const float normalized =
      inverted ? (end - coordinate) / span : (coordinate - start) / span;
  return std::clamp(normalized, 0.0f, 1.0f);
}

SliderTrackAxis slider_track_axis(Rect bounds, SliderOrientation orientation,
                                  bool has_formatter) noexcept {
  if (orientation == SliderOrientation::Horizontal) {
    const float start = bounds.x + 8.0f;
    const float end = std::max(start, bounds.x + bounds.w - 8.0f);
    return SliderTrackAxis{start, end, false};
  }

  const float track_bottom =
      has_formatter ? bounds.y + bounds.h - 18.0f : bounds.y + bounds.h;
  const float start = bounds.y + 8.0f;
  const float end = std::max(start, track_bottom - 8.0f);
  return SliderTrackAxis{start, end, true};
}

SliderComponent::SliderComponent(Binding<float> state, float minimum,
                                 float maximum, float step,
                                 SliderOrientation orientation,
                                 Formatter formatter, SliderStyle style)
    : state_(std::move(state)), domain_(minimum, maximum, step),
      orientation_(orientation), formatter_(std::move(formatter)),
      style_(std::move(style)) {}

bool SliderComponent::focusable() const noexcept { return true; }

Size SliderComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto &theme = current_theme();
  const auto resolved = resolved_style(focused_);
  const float styled_cross_axis =
      resolved.thumb_diameter + resolved.focus_ring_width * 2.0f;
  const float compact_cross_axis =
      std::max(theme.controls.minimum_hit_target, styled_cross_axis);
  if (orientation_ == SliderOrientation::Horizontal) {
    const float height =
        formatter_ ? std::max(theme.controls.control_height + theme.spacing.sm,
                              compact_cross_axis)
                   : compact_cross_axis;
    return Size{160.0f, height};
  }
  const float width =
      formatter_
          ? std::max(theme.controls.control_height + theme.spacing.large * 2.0f,
                     compact_cross_axis)
          : compact_cross_axis;
  return Size{width, 160.0f};
}

void SliderComponent::mount(MountContext &context) {
  auto invalidate = context.invalidator();
  subscription_ = state_.observe(
      [invalidate = std::move(invalidate)](const float &) { invalidate(); });
}

void SliderComponent::unmount(LifecycleContext &) { subscription_.reset(); }

void SliderComponent::focus_changed(bool focused, FocusContext &context) {
  if (focused_ == focused)
    return;
  const auto before = resolved_style(focused_);
  focused_ = focused;
  invalidate_style_transition(before, resolved_style(focused_), context, true);
}

void SliderComponent::deactivate(LifecycleContext &context) {
  if (!dragging_ && !hovered_ && !focused_)
    return;
  const auto before = resolved_style(focused_);
  const bool focus_changed = focused_;
  dragging_ = false;
  hovered_ = false;
  focused_ = false;
  invalidate_style_transition(before, resolved_style(focused_), context,
                              focus_changed);
}

EventResult SliderComponent::input(const InputEvent &event,
                                   InputContext &context) {
  if ((effective_read_only() || !state_.valid()) && mutating_input(event)) {
    const auto before = resolved_style(focused_);
    const bool release_capture = dragging_;
    dragging_ = false;
    if (event.type == InputType::PointerDown ||
        event.type == InputType::PointerMove ||
        event.type == InputType::PointerUp) {
      hovered_ = context.bounds().contains(event.position);
    }
    if (release_capture)
      context.release_pointer();
    invalidate_style_transition(before, resolved_style(focused_), context);
    return EventResult::Handled;
  }

  switch (event.type) {
  case InputType::PointerDown:
    return begin_drag(event, context);

  case InputType::PointerMove:
    return pointer_move(event, context);

  case InputType::PointerUp:
    if (!dragging_)
      return EventResult::Ignored;
    return update_drag(event, context, true);

  case InputType::PointerCancel: {
    if (!dragging_)
      return EventResult::Ignored;
    const auto before = resolved_style(focused_);
    dragging_ = false;
    hovered_ = false;
    invalidate_style_transition(before, resolved_style(focused_), context);
    return EventResult::Handled;
  }

  case InputType::KeyDown:
    return key_down(event, context);

  default:
    return EventResult::Ignored;
  }
}

SliderVisualState SliderComponent::visual_state(bool focused) const noexcept {
  return slider_visual_state(effective_enabled(), effective_read_only(),
                             hovered_, dragging_, focused);
}

SemanticInfo SliderComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Slider;
  info.numeric_value = domain_.effective_external(state_.get());
  info.value_range =
      SemanticValueRange{domain_.minimum(), domain_.maximum(), domain_.step()};
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only() || !state_.valid();
  if (info.enabled) {
    info.actions.push_back(SemanticAction::Focus);
    if (!info.read_only) {
      info.actions.push_back(SemanticAction::Increment);
      info.actions.push_back(SemanticAction::Decrement);
      info.actions.push_back(SemanticAction::SetValue);
    }
  }
  return info;
}

void SliderComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const float fraction = domain_.fraction(state_.get());
  const auto resolved = resolved_style(context.focused());
  const auto axis =
      slider_track_axis(bounds, orientation_, static_cast<bool>(formatter_));
  const float track_thickness = resolved.track_thickness;
  const float half_track = track_thickness * 0.5f;
  const float thumb_radius = resolved.thumb_diameter * 0.5f;
  const float focus_radius = thumb_radius + resolved.focus_ring_width;

  auto &painter = context.painter();
  if (orientation_ == SliderOrientation::Horizontal) {
    const float track_bottom =
        formatter_ ? bounds.y + bounds.h - 16.0f : bounds.y + bounds.h;
    const float y = bounds.y + (track_bottom - bounds.y) * 0.5f;
    const float x0 = axis.start;
    const float x1 = axis.end;
    const float thumb_x = axis.position(fraction);
    painter.fill_rounded_rect(
        Rect{x0, y - half_track, x1 - x0, track_thickness}, half_track,
        resolved.track);
    painter.fill_rounded_rect(
        Rect{x0, y - half_track, thumb_x - x0, track_thickness}, half_track,
        resolved.active);
    if (context.focused()) {
      painter.circle(Point{thumb_x, y}, focus_radius, resolved.focus_ring);
    }
    painter.circle(Point{thumb_x, y}, thumb_radius, resolved.thumb);
    if (formatter_) {
      const auto text = formatter_(domain_.effective_external(state_.get()));
      painter.text(
          Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h - 7.0f}, text,
          11.0f, resolved.formatter_text, TextAlign::Center);
    }
  } else {
    const float x = bounds.x + bounds.w * 0.5f;
    const float y0 = axis.start;
    const float y1 = axis.end;
    const float thumb_y = axis.position(fraction);
    painter.fill_rounded_rect(
        Rect{x - half_track, y0, track_thickness, y1 - y0}, half_track,
        resolved.track);
    painter.fill_rounded_rect(
        Rect{x - half_track, thumb_y, track_thickness, y1 - thumb_y},
        half_track, resolved.active);
    if (context.focused()) {
      painter.circle(Point{x, thumb_y}, focus_radius, resolved.focus_ring);
    }
    painter.circle(Point{x, thumb_y}, thumb_radius, resolved.thumb);
    if (formatter_) {
      const auto text = formatter_(domain_.effective_external(state_.get()));
      painter.text(
          Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h - 7.0f}, text,
          11.0f, resolved.formatter_text, TextAlign::Center);
    }
  }
}

template <class Context>
void SliderComponent::invalidate_style_transition(
    const ResolvedSliderStyle &before, const ResolvedSliderStyle &after,
    Context &context, bool force_paint) const {
  switch (classify_slider_style_change(before, after)) {
  case SliderStyleInvalidation::None:
    if (force_paint)
      context.invalidate();
    return;
  case SliderStyleInvalidation::Paint:
    context.invalidate();
    return;
  case SliderStyleInvalidation::Layout:
    context.invalidate_layout();
    return;
  }
}

bool SliderComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  return classify_slider_style_change(resolved_style(before, focused_),
                                      resolved_style(after, focused_)) ==
         SliderStyleInvalidation::Layout;
}

VisualState
SliderComponent::current_visual_state(ComponentAvailability availability,
                                      bool focused) const noexcept {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
      .hovered = hovered_,
      .pressed = dragging_,
      .focused = focused,
  };
}

VisualState SliderComponent::current_visual_state(bool focused) const noexcept {
  return current_visual_state(effective_availability(), focused);
}

ResolvedSliderStyle
SliderComponent::resolved_style(ComponentAvailability availability,
                                bool focused) const {
  return resolve_slider_style(default_slider_style(current_theme()), style_,
                              current_visual_state(availability, focused));
}

ResolvedSliderStyle SliderComponent::resolved_style(bool focused) const {
  return resolved_style(effective_availability(), focused);
}

bool SliderComponent::mutating_input(const InputEvent &event) const noexcept {
  if (event.type == InputType::PointerDown)
    return true;
  if (event.type == InputType::PointerMove ||
      event.type == InputType::PointerUp) {
    return dragging_;
  }
  if (event.type != InputType::KeyDown)
    return false;
  return event.key == Key::Left || event.key == Key::Right ||
         event.key == Key::Up || event.key == Key::Down ||
         event.key == Key::Home || event.key == Key::End;
}

float SliderComponent::value_from_pointer(
    const InputEvent &event, const InputContext &context) const noexcept {
  const auto axis = slider_track_axis(context.bounds(), orientation_,
                                      static_cast<bool>(formatter_));
  const float coordinate = orientation_ == SliderOrientation::Horizontal
                               ? event.position.x
                               : event.position.y;
  return domain_.value_from_fraction(axis.fraction(coordinate));
}

EventResult SliderComponent::begin_drag(const InputEvent &event,
                                        InputContext &context) {
  const auto before = resolved_style(focused_);
  const float target = value_from_pointer(event, context);
  auto state = state_;
  dragging_ = true;
  hovered_ = true;
  context.capture_pointer();
  invalidate_style_transition(before, resolved_style(focused_), context);
  if (state.valid() && InputMutationAccess::allowed(context) && state.get() != target)
    state.set(target);
  return EventResult::Handled;
}

EventResult SliderComponent::pointer_move(const InputEvent &event,
                                          InputContext &context) {
  const auto before = resolved_style(focused_);
  const bool inside = context.bounds().contains(event.position);
  hovered_ = inside;
  if (!dragging_) {
    invalidate_style_transition(before, resolved_style(focused_), context);
    return EventResult::Handled;
  }
  return update_drag(event, context, false, before);
}

EventResult SliderComponent::update_drag(const InputEvent &event,
                                         InputContext &context, bool release,
                                         const ResolvedSliderStyle &before) {
  const auto effective_before = release ? resolved_style(focused_) : before;
  const float target = value_from_pointer(event, context);
  auto state = state_;
  if (release) {
    hovered_ = context.bounds().contains(event.position);
    dragging_ = false;
    context.release_pointer();
  }
  invalidate_style_transition(effective_before, resolved_style(focused_),
                              context);
  if (state.valid() && InputMutationAccess::allowed(context) && state.get() != target)
    state.set(target);
  return EventResult::Handled;
}

EventResult SliderComponent::key_down(const InputEvent &event, InputContext &context) {
  auto state = state_;
  float target = state.get();
  const float effective = domain_.effective_external(target);
  const double increment = domain_.keyboard_increment(event.shift);

  switch (event.key) {
  case Key::Left:
  case Key::Down:
    target = domain_.normalize(static_cast<double>(effective) - increment);
    break;
  case Key::Right:
  case Key::Up:
    target = domain_.normalize(static_cast<double>(effective) + increment);
    break;
  case Key::Home:
    target = domain_.normalize(domain_.minimum());
    break;
  case Key::End:
    target = domain_.normalize(domain_.maximum());
    break;
  default:
    return EventResult::Ignored;
  }

  if (state.valid() && InputMutationAccess::allowed(context) && state.get() != target)
    state.set(target);
  return EventResult::Handled;
}
} // namespace detail

Slider::Slider(Binding<float> state) : state_(std::move(state)) {}

Slider::Slider(State<float> &state) : Slider(state.binding()) {}

Slider &&Slider::range(float minimum, float maximum) && {
  minimum_ = minimum;
  maximum_ = maximum;
  return std::move(*this);
}

Slider &&Slider::step(float value) && {
  step_ = value;
  return std::move(*this);
}

Slider &&Slider::orientation(SliderOrientation value) && {
  orientation_ = value;
  return std::move(*this);
}

Slider &&Slider::formatter(Formatter value) && {
  formatter_ = std::move(value);
  return std::move(*this);
}

Slider &&Slider::style(SliderStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec Slider::spec() && {
  auto state = std::move(state_);
  const float minimum = minimum_;
  const float maximum = maximum_;
  const float step = step_;
  const auto orientation = orientation_;
  auto formatter = std::move(formatter_);
  auto style = std::move(style_);
  return Spec{[state = std::move(state), minimum, maximum, step, orientation,
               formatter = std::move(formatter),
               style = std::move(style)]() mutable {
                return std::make_unique<detail::SliderComponent>(
                    std::move(state), minimum, maximum, step, orientation,
                    std::move(formatter), std::move(style));
              },
              {}};
}

} // namespace ui
