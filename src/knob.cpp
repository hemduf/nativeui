#include <nativeui/knob.hpp>

#include <stdexcept>

namespace ui {

KnobComponent::KnobComponent(std::string label, Binding<float> state,
                             float minimum, float maximum)
    : label_(std::move(label)), state_(std::move(state)), minimum_(minimum),
      maximum_(maximum), observation_(std::make_shared<Observation>()) {
  if (!std::isfinite(minimum_) || !std::isfinite(maximum_))
    throw std::invalid_argument("Knob range must be finite");
  if (maximum_ <= minimum_) {
    maximum_ = minimum_ + 1.0f;
    if (!(maximum_ > minimum_))
      maximum_ =
          std::nextafter(minimum_, std::numeric_limits<float>::infinity());
    if (!std::isfinite(maximum_))
      throw std::invalid_argument("Knob range has no finite successor");
  }
  value_ = effective_value(state_.get());
  observation_->value = value_;
}

float KnobComponent::effective_value(float value) const noexcept {
  if (!std::isfinite(value))
    return minimum_;
  return std::clamp(value, minimum_, maximum_);
}

bool KnobComponent::focusable() const noexcept { return true; }

bool KnobComponent::cancel_capture_on_read_only() const noexcept {
  return true;
}

Size KnobComponent::measure(const std::vector<ChildMetrics> &) const {
  return {176.0f, 182.0f};
}

SemanticInfo KnobComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Slider;
  info.name = label_;
  info.numeric_value = effective_value(state_.get());
  const double step = (static_cast<double>(maximum_) - minimum_) * 0.01;
  info.value_range = SemanticValueRange{minimum_, maximum_, step};
  info.enabled = effective_enabled();
  info.read_only = effective_read_only() || !state_.valid();
  info.focusable = true;
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

void KnobComponent::mount(MountContext &ctx) {
  auto observation = observation_;
  observation->active = true;
  observation->value = effective_value(state_.get());
  auto invalidate = ctx.invalidator();
  const float minimum = minimum_;
  const float maximum = maximum_;
  subscription_ = state_.observe([observation = std::move(observation),
                                  invalidate = std::move(invalidate), minimum,
                                  maximum](const float &value) {
    if (!observation->active)
      return;
    const float effective =
        std::isfinite(value) ? std::clamp(value, minimum, maximum) : minimum;
    if (observation->dragging &&
        (!observation->writing || value != observation->expected)) {
      observation->cancel_requested = false;
      observation->dragging = false;
      ++observation->generation;
      (void)observation->gesture.cancel();
      auto release = std::move(observation->release_pointer);
      if (release)
        release();
    }
    observation->value = effective;
    invalidate();
  });
}

void KnobComponent::unmount(LifecycleContext &) {
  observation_->active = false;
  observation_->dragging = false;
  observation_->cancel_requested = false;
  observation_->release_pointer = {};
  subscription_.reset();
  ++observation_->generation;
  (void)observation_->gesture.cancel();
}

void KnobComponent::deactivate(LifecycleContext &ctx) {
  const bool was_active = observation_->gesture.active();
  ++observation_->generation;
  (void)observation_->gesture.cancel();
  observation_->dragging = false;
  observation_->cancel_requested = false;
  observation_->release_pointer = {};
  if (was_active)
    ctx.invalidate();
}

void KnobComponent::focus_changed(bool focused, FocusContext &ctx) {
  if (focused || !observation_->gesture.active())
    return;
  auto observation = observation_;
  ++observation->generation;
  (void)observation->gesture.cancel();
  observation->dragging = false;
  observation->cancel_requested = false;
  auto release = std::move(observation->release_pointer);
  if (release)
    release();
  ctx.invalidate();
}

void KnobComponent::publish(double value, InputContext &ctx) {
  auto state = state_;
  auto observation = observation_;
  const auto generation = observation->generation;
  const float next = static_cast<float>(std::clamp(
      value, static_cast<double>(minimum_), static_cast<double>(maximum_)));
  if (state.get() == next)
    return;
  struct WriteScope {
    std::shared_ptr<Observation> observation;
    bool previous_writing;
    float previous_expected;
    ~WriteScope() noexcept {
      observation->writing = previous_writing;
      observation->expected = previous_expected;
    }
  } scope{observation, observation->writing, observation->expected};
  observation->writing = true;
  observation->expected = next;
  std::function<void()> release;
  // The scope owns detached bookkeeping. State observers may remove the
  // retained node; no component/context access follows the publication.
  try {
    release = ctx.pointer_releaser();
    ctx.invalidate();
    if (state.valid() && observation->generation == generation &&
        detail::InputMutationAccess::allowed(ctx))
      state.set(next);
  } catch (...) {
    if (observation->generation == generation) {
      ++observation->generation;
      (void)observation->gesture.cancel();
      observation->dragging = false;
      observation->cancel_requested = false;
      auto armed_release = std::move(observation->release_pointer);
      // Both gesture and capture carry the contact generation. An observer
      // may already have started a newer contact while this publication failed.
      try {
        if (release)
          release();
        else if (armed_release)
          armed_release();
      } catch (...) {
      }
    }
    throw;
  }
}

EventResult KnobComponent::input(const InputEvent &event, InputContext &ctx) {
  const float current = effective_value(state_.get());
  if (observation_->gesture.active() && current != observation_->value)
    observation_->cancel_requested = true;
  observation_->value = current;
  value_ = current;
  const bool read_only = effective_read_only() || !state_.valid();
  if (observation_->gesture.active() &&
      (read_only || observation_->cancel_requested)) {
    ++observation_->generation;
    (void)observation_->gesture.cancel();
    observation_->dragging = false;
    observation_->cancel_requested = false;
    auto release = std::move(observation_->release_pointer);
    if (release)
      release();
    else
      ctx.release_pointer();
    ctx.invalidate();
    return EventResult::Handled;
  }
  const double range = static_cast<double>(maximum_) - minimum_;
  if (event.type == InputType::KeyDown) {
    double direction = 0.0;
    if (event.key == Key::Left || event.key == Key::Down)
      direction = -1.0;
    if (event.key == Key::Right || event.key == Key::Up)
      direction = 1.0;
    if (direction == 0.0)
      return EventResult::Ignored;
    if (!read_only)
      publish(static_cast<double>(current) +
                  direction * range * (event.shift ? 0.002 : 0.01),
              ctx);
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerDown) {
    if (read_only)
      return EventResult::Handled;
    auto release = ctx.pointer_releaser();
    ++observation_->generation;
    observation_->gesture.begin(event.position);
    drag_start_value_ = current;
    observation_->dragging = true;
    observation_->cancel_requested = false;
    ctx.capture_pointer();
    observation_->release_pointer = std::move(release);
    ctx.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerMove && observation_->gesture.active()) {
    const auto drag = observation_->gesture.move(event.position);
    if (drag.dragging) {
      const double delta = -static_cast<double>(drag.total.y) * range / 180.0;
      if (std::isfinite(delta))
        publish(static_cast<double>(drag_start_value_) + delta, ctx);
    } else {
      ctx.invalidate();
    }
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerUp && observation_->gesture.active()) {
    ++observation_->generation;
    (void)observation_->gesture.end(event.position);
    observation_->dragging = false;
    observation_->release_pointer = {};
    ctx.release_pointer();
    ctx.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerCancel &&
      observation_->gesture.active()) {
    ++observation_->generation;
    (void)observation_->gesture.cancel();
    observation_->dragging = false;
    observation_->release_pointer = {};
    ctx.release_pointer();
    ctx.invalidate();
    return EventResult::Handled;
  }
  return EventResult::Ignored;
}

void KnobComponent::paint(PaintContext &p) const {
  const auto b = p.bounds();
  auto &dl = p.painter();
  dl.fill_rounded_rect(b, 14.0f, colors::panel);
  dl.stroke_rounded_rect(b, 14.0f, p.focused() ? 2.0f : 1.0f,
                         p.focused() ? colors::borderFocus : colors::border);
  dl.text(Point{b.x + b.w * 0.5f, b.y + 24.0f}, label_, 13.0f, colors::text,
          TextAlign::Center);

  const Point center{b.x + b.w * 0.5f, b.y + 93.0f};
  constexpr float radius = 42.0f;
  constexpr float arc_radius = 51.0f;
  constexpr float start = 0.75f * kPi;
  constexpr float sweep = 1.5f * kPi;
  const float effective = effective_value(state_.get());
  const float normalized = static_cast<float>(
      std::clamp((static_cast<double>(effective) - minimum_) /
                     (static_cast<double>(maximum_) - minimum_),
                 0.0, 1.0));
  const float angle = start + normalized * sweep;

  dl.arc(center, arc_radius, start, start + sweep, 4.0f, colors::track);
  if (normalized > 0.001f)
    dl.arc(center, arc_radius, start, angle, 4.0f, colors::accent);
  dl.circle(center, radius, colors::knob);
  dl.circle(center, radius - 7.0f, colors::knobInner);
  const Point marker{center.x + std::cos(angle) * (radius - 13.0f),
                     center.y + std::sin(angle) * (radius - 13.0f)};
  dl.line(center, marker, 3.0f, colors::text);

  std::ostringstream value;
  value << std::fixed << std::setprecision(2) << effective;
  dl.text(Point{b.x + b.w * 0.5f, b.y + b.h - 22.0f}, value.str(), 12.0f,
          colors::textMuted, TextAlign::Center);
}

Knob::Knob(std::string label, Binding<float> state)
    : label_(std::move(label)), state_(std::move(state)) {}

Knob::Knob(std::string label, State<float> &state)
    : Knob(std::move(label), state.binding()) {}

Knob &&Knob::range(float minimum, float maximum) && {
  minimum_ = minimum;
  maximum_ = maximum;
  return std::move(*this);
}

Spec Knob::spec() && {
  auto label = std::move(label_);
  auto state = std::move(state_);
  const float minimum = minimum_;
  const float maximum = maximum_;
  return Spec{[label = std::move(label), state = std::move(state), minimum,
               maximum]() mutable {
                return std::make_unique<KnobComponent>(
                    std::move(label), std::move(state), minimum, maximum);
              },
              {}};
}

} // namespace ui
