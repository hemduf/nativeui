#include <nativeui/slider.hpp>

namespace ui {

namespace detail {

namespace {
template <class T, class Cleanup>
void cancel_after_cleanup(const std::shared_ptr<EditSession<T>> &edit,
                          Cleanup &&cleanup) {
  try {
    std::forward<Cleanup>(cleanup)();
  } catch (...) {
    try {
      edit->cancel();
    } catch (...) {
    }
    throw;
  }
  edit->cancel();
}
} // namespace
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
    : SliderComponent(std::move(state), minimum, maximum, step, orientation,
                      std::move(formatter), std::move(style), {}, false) {}

SliderComponent::SliderComponent(Binding<float> state, float minimum,
                                 float maximum, float step,
                                 SliderOrientation orientation,
                                 Formatter formatter, SliderStyle style,
                                 EditCallbacks<float> callbacks,
                                 bool wheel_enabled)
    : state_(std::move(state)),
      edit_(std::make_shared<EditSession<float>>(state_, std::move(callbacks))),
      interaction_(std::make_shared<Interaction>()),
      wheel_enabled_(wheel_enabled), domain_(minimum, maximum, step),
      orientation_(orientation), formatter_(std::move(formatter)),
      style_(std::move(style)) {
  interaction_->owner = this;
}

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
  interaction_->owner = this;
  auto invalidate = context.invalidator();
  subscription_ = state_.observe(
      [invalidate = std::move(invalidate)](const float &) { invalidate(); });
}

void SliderComponent::unmount(LifecycleContext &) {
  const auto interaction = interaction_;
  const auto edit = edit_;
  interaction->owner = nullptr;
  ++interaction->generation;
  interaction->release_pointer = {};
  dragging_ = false;
  subscription_.reset();
  try {
    edit->cancel();
  } catch (...) {
  }
}

void SliderComponent::focus_changed(bool focused, FocusContext &context) {
  if (focused_ == focused)
    return;
  const auto edit = edit_;
  const auto interaction = interaction_;
  const auto before = resolved_style(focused_);
  focused_ = focused;
  std::function<void()> release;
  if (!focused) {
    ++interaction->generation;
    dragging_ = false;
    release = std::move(interaction->release_pointer);
  }
  const auto after = resolved_style(focused_);
  if (focused) {
    invalidate_style_transition(before, after, context, true);
  } else {
    cancel_after_cleanup(edit, [&] {
      try {
        invalidate_style_transition(before, after, context, true);
      } catch (...) {
        try {
          if (release)
            release();
        } catch (...) {
        }
        throw;
      }
      if (release)
        release();
    });
  }
}

void SliderComponent::deactivate(LifecycleContext &context) {
  const auto edit = edit_;
  const auto interaction = interaction_;
  if (!dragging_ && !hovered_ && !focused_ && !edit->active())
    return;
  const auto before = resolved_style(focused_);
  const bool focus_changed = focused_;
  ++interaction->generation;
  auto release = std::move(interaction->release_pointer);
  dragging_ = false;
  hovered_ = false;
  focused_ = false;
  const auto after = resolved_style(focused_);
  cancel_after_cleanup(edit, [&] {
    try {
      invalidate_style_transition(before, after, context, focus_changed);
    } catch (...) {
      try {
        if (release)
          release();
      } catch (...) {
      }
      throw;
    }
    if (release)
      release();
  });
}

EventResult SliderComponent::input(const InputEvent &event,
                                   InputContext &context) {
  const auto interaction = interaction_;
  const auto edit = edit_;
  if (event.type == InputType::PointerDown)
    ++interaction->generation;
  const auto generation = interaction->generation;
  auto release = context.pointer_releaser();
  try {
    if (event.type == InputType::KeyDown && event.key == Key::Escape &&
        dragging_) {
      const auto before = resolved_style(focused_);
      dragging_ = false;
      if (interaction->release_pointer)
        release = std::move(interaction->release_pointer);
      invalidate_style_transition(before, resolved_style(focused_), context);
      if (release)
        release();
      edit->cancel();
      return EventResult::Handled;
    }
    if ((effective_read_only() || !state_.valid()) && mutating_input(event)) {
      const auto before = resolved_style(focused_);
      const bool release_capture = dragging_;
      dragging_ = false;
      if (release_capture && interaction->release_pointer)
        release = std::move(interaction->release_pointer);
      if (event.type == InputType::PointerDown ||
          event.type == InputType::PointerMove ||
          event.type == InputType::PointerUp)
        hovered_ = context.bounds().contains(event.position);
      invalidate_style_transition(before, resolved_style(focused_), context);
      if (release_capture && release)
        release();
      edit->cancel();
      return EventResult::Handled;
    }
    switch (event.type) {
    case InputType::PointerDown:
      interaction->release_pointer = release;
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
      if (interaction->release_pointer)
        release = std::move(interaction->release_pointer);
      invalidate_style_transition(before, resolved_style(focused_), context);
      if (release)
        release();
      edit->cancel();
      return EventResult::Handled;
    }
    case InputType::KeyDown:
      return key_down(event, context);
    case InputType::PointerWheel: {
      if (!wheel_enabled_ || !std::isfinite(event.delta.y))
        return EventResult::Ignored;
      const auto target = domain_.normalize(
          domain_.effective_external(state_.get()) +
          event.delta.y * domain_.keyboard_increment(event.shift));
      auto permission = InputMutationAccess::guard(context);
      if (!effective_read_only() && state_.valid() &&
          (!permission || permission()))
        edit->set(target, EditSource::Wheel);
      return EventResult::Handled;
    }
    default:
      return EventResult::Ignored;
    }
  } catch (...) {
    // The callback may already have retired this component or started a newer
    // contact. Restore only matching detached interaction state.
    if (interaction->owner && interaction->generation == generation) {
      interaction->owner->dragging_ = false;
      // Keyboard/wheel contexts do not carry the originating touch contact.
      // Prefer its retained release action, including when invalidation throws.
      if (interaction->release_pointer)
        release = std::move(interaction->release_pointer);
      ++interaction->generation;
      try {
        if (release)
          release();
      } catch (...) {
      }
      try {
        edit->cancel();
      } catch (...) {
      }
    }
    throw;
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
  if (event.type == InputType::PointerWheel)
    return wheel_enabled_;
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
  const auto interaction = interaction_;
  const auto generation = interaction->generation;
  const auto edit = edit_;
  const auto state = state_;
  auto permission = InputMutationAccess::guard(context);
  const auto before = resolved_style(focused_);
  const float target = value_from_pointer(event, context);
  dragging_ = true;
  hovered_ = true;
  context.capture_pointer();
  if (!interaction->owner || interaction->generation != generation)
    return EventResult::Handled;
  invalidate_style_transition(before, resolved_style(focused_), context);
  if (interaction->owner && interaction->generation == generation &&
      state.valid() && (!permission || permission()) &&
      edit->begin(EditSource::Pointer)) {
    if (interaction->owner && interaction->generation == generation &&
        (!permission || permission()))
      edit->update(target);
    else
      edit->cancel();
  }
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
  const auto interaction = interaction_;
  const auto generation = interaction->generation;
  const auto edit = edit_;
  const auto state = state_;
  auto permission = InputMutationAccess::guard(context);
  auto release_contact = context.pointer_releaser();
  const auto effective_before = release ? resolved_style(focused_) : before;
  const float target = value_from_pointer(event, context);
  if (release) {
    hovered_ = context.bounds().contains(event.position);
    dragging_ = false;
    interaction->release_pointer = {};
  }
  invalidate_style_transition(effective_before, resolved_style(focused_),
                              context);
  if (release && release_contact)
    release_contact();
  if (interaction->owner && interaction->generation == generation &&
      state.valid() && (!permission || permission())) {
    if (release)
      edit->finish(target);
    else
      edit->update(target);
  } else if (release && interaction->generation == generation) {
    edit->cancel();
  }
  return EventResult::Handled;
}

EventResult SliderComponent::key_down(const InputEvent &event,
                                      InputContext &context) {
  const auto state = state_;
  const auto edit = edit_;
  auto permission = InputMutationAccess::guard(context);
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
  if (state.valid() && (!permission || permission()))
    edit->set(target, EditSource::Keyboard);
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

Slider &&Slider::on_edit(EditCallbacks<float> callbacks) && {
  callbacks_ = std::move(callbacks);
  return std::move(*this);
}

Slider &&Slider::wheel_enabled(bool value) && {
  wheel_enabled_ = value;
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
  auto callbacks = std::move(callbacks_);
  const bool wheel_enabled = wheel_enabled_;
  return Spec{[state = std::move(state), minimum, maximum, step, orientation,
               formatter = std::move(formatter), style = std::move(style),
               callbacks = std::move(callbacks), wheel_enabled]() mutable {
                return std::make_unique<detail::SliderComponent>(
                    std::move(state), minimum, maximum, step, orientation,
                    std::move(formatter), std::move(style),
                    std::move(callbacks), wheel_enabled);
              },
              {}};
}

} // namespace ui
