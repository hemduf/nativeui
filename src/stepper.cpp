#include "detail/widget_stepper_policy.hpp"
#include <nativeui/stepper.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_range(double minimum, double maximum) {
  if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
    throw std::invalid_argument(
        "Stepper range must be finite with minimum <= maximum");
}
void validate_step(double value) {
  if (!std::isfinite(value) || value <= 0.0)
    throw std::invalid_argument("Stepper step must be finite and positive");
}
void validate_patch(const StepperStylePatch &patch) {
  for (const auto &value :
       {patch.width, patch.height, patch.border_width, patch.corner_radius})
    if (value && (!std::isfinite(*value) || *value < 0.0))
      throw std::invalid_argument(
          "Stepper style metrics must be finite and nonnegative");
}
void validate_style(const StepperStyle &style) {
  for (const auto *patch : {&style.base, &style.hovered, &style.pressed,
                            &style.focused, &style.disabled, &style.read_only})
    validate_patch(*patch);
}
float logical(double value) noexcept {
  return static_cast<float>(
      std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}
int cell_at(Rect bounds, Point point) noexcept {
  if (bounds.empty() || !bounds.contains(point))
    return 0;
  return point.y <= bounds.y + bounds.h * 0.5f ? 1 : -1;
}
} // namespace
namespace detail {
double StepperAccess::stepped_value(double value, double minimum_value,
                                    double maximum_value, double step_value,
                                    int direction) noexcept {
  const long double minimum = minimum_value;
  const long double step = step_value;
  const long double current =
      std::isfinite(value) ? std::clamp(value, minimum_value, maximum_value)
                           : minimum_value;
  const long double next = current + direction * step;
  const long double snapped =
      minimum + std::round((next - minimum) / step) * step;
  if (!std::isfinite(snapped))
    return direction > 0 ? maximum_value : minimum_value;
  return static_cast<double>(
      std::clamp(snapped, minimum, static_cast<long double>(maximum_value)));
}
struct StepperRepeat {
  std::function<void()> request_editor_focus;
  std::function<void(double, std::function<bool()>)> publish_value;
  std::function<bool()> mutation_guard;
  explicit StepperRepeat(Binding<double> value) : source(std::move(value)) {}
  Binding<double> source;
  double minimum{};
  double maximum{};
  double step{1.0};
  Dispatcher dispatcher;
  TimerHandle timer;
  std::function<void()> invalidate;
  std::function<void()> invalidate_layout;
  std::function<void()> release;
  std::uint64_t contact_generation{};
  std::uint64_t timer_generation{};
  bool mounted{};
  bool active{};
  bool allowed{true};
  bool armed{};
  bool inside{};
  int direction{};
};
namespace {
double effective(const StepperRepeat &repeat) noexcept {
  const double value = repeat.source.get();
  return std::isfinite(value)
             ? std::clamp(value, repeat.minimum, repeat.maximum)
             : repeat.minimum;
}
double incremented(const StepperRepeat &repeat, int direction) noexcept {
  return StepperAccess::stepped_value(effective(repeat), repeat.minimum,
                                      repeat.maximum, repeat.step, direction);
}
bool can_change(const StepperRepeat &repeat, int direction) noexcept {
  return repeat.source.valid() && repeat.minimum < repeat.maximum &&
         incremented(repeat, direction) != effective(repeat);
}
void cancel_timer(const std::shared_ptr<StepperRepeat> &repeat) noexcept {
  ++repeat->timer_generation;
  (void)repeat->dispatcher.cancel(repeat->timer);
  repeat->timer = {};
}
void end_contact(const std::shared_ptr<StepperRepeat> &repeat) noexcept {
  ++repeat->contact_generation;
  repeat->armed = false;
  repeat->inside = false;
  repeat->direction = 0;
  repeat->mutation_guard = {};
  cancel_timer(repeat);
  auto release = std::move(repeat->release);
  if (release) {
    try {
      release();
    } catch (...) {
    }
  }
}
bool can_repeat(const StepperRepeat &repeat) noexcept {
  return repeat.mounted && repeat.active && repeat.allowed && repeat.armed &&
         repeat.inside && repeat.dispatcher.valid() &&
         can_change(repeat, repeat.direction);
}
void publish(const std::shared_ptr<StepperRepeat> &repeat, double value,
             bool layout = false) {
  auto source = repeat->source;
  auto guard = repeat->mutation_guard;
  auto writer = repeat->publish_value;
  const auto generation = repeat->contact_generation;
  try {
    auto invalidate = layout ? repeat->invalidate_layout : repeat->invalidate;
    if (invalidate)
      invalidate();
    if (repeat->mounted && repeat->contact_generation == generation &&
        source.valid() && repeat->allowed && (!guard || guard())) {
      if (writer)
        writer(value, std::move(guard));
      else
        source.set(value);
    }
  } catch (...) {
    if (repeat->contact_generation == generation)
      end_contact(repeat);
    throw;
  }
}
void invalidate_input(const std::shared_ptr<StepperRepeat> &repeat,
                      InputContext &context, bool layout) {
  const auto generation = repeat->contact_generation;
  try {
    if (layout)
      context.invalidate_layout();
    else
      context.invalidate();
  } catch (...) {
    if (repeat->contact_generation == generation)
      end_contact(repeat);
    throw;
  }
}
void arm_timer(const std::shared_ptr<StepperRepeat> &repeat,
               DispatcherDuration delay) noexcept {
  cancel_timer(repeat);
  if (!can_repeat(*repeat))
    return;
  const auto generation = repeat->timer_generation;
  const std::weak_ptr<StepperRepeat> weak = repeat;
  try {
    repeat->timer =
        repeat->dispatcher.schedule_after(delay, [weak, generation] {
          const auto current = weak.lock();
          if (!current || current->timer_generation != generation)
            return;
          current->timer = {};
          if (!current->source.valid()) {
            end_contact(current);
            return;
          }
          if (!can_repeat(*current))
            return;
          const double value = incremented(*current, current->direction);
          // Schedule one future deadline before application publication. The
          // detached runtime survives removal and its contact generation
          // protects any newer gesture that an observer may start reentrantly.
          if (current->direction > 0 ? value < current->maximum
                                     : value > current->minimum)
            arm_timer(current, DispatcherDuration{0.08});
          else
            cancel_timer(current);
          publish(current, value);
        });
  } catch (...) {
    repeat->timer = {};
  }
}
} // namespace
StepperComponent::StepperComponent(Binding<double> value, std::string label,
                                   double minimum, double maximum, double step,
                                   StepperStyle style)
    : label_(std::move(label)), style_(std::move(style)),
      repeat_(std::make_shared<StepperRepeat>(std::move(value))) {
  validate_range(minimum, maximum);
  validate_step(step);
  validate_style(style_);
  repeat_->minimum = minimum;
  repeat_->maximum = maximum;
  repeat_->step = step;
}
void StepperAccess::configure(
    StepperComponent &editor, bool focusable, std::function<void()> request,
    std::function<void(double, std::function<bool()>)> writer) {
  if (editor.repeat_->mounted)
    throw std::logic_error("Stepper policy must be configured before mount");
  editor.focusable_override_ = focusable;
  editor.repeat_->request_editor_focus = std::move(request);
  editor.repeat_->publish_value = std::move(writer);
}
bool StepperComponent::focusable() const noexcept {
  return focusable_override_ && repeat_->minimum < repeat_->maximum;
}
bool StepperComponent::cancel_capture_on_read_only() const noexcept {
  return true;
}
StepperComponent::Presentation
StepperComponent::presentation(ComponentAvailability availability,
                               int cell) const noexcept {
  const auto &theme = current_theme();
  Presentation value{theme.palette.surface,
                     theme.palette.text,
                     theme.palette.border,
                     24.0f,
                     theme.controls.control_height,
                     theme.controls.border_width,
                     theme.radii.sm};
  const auto apply = [&](const StepperStylePatch &patch) {
    if (patch.fill)
      value.fill = *patch.fill;
    if (patch.arrow)
      value.arrow = *patch.arrow;
    if (patch.border)
      value.border = *patch.border;
    if (patch.width)
      value.width = logical(*patch.width);
    if (patch.height)
      value.height = logical(*patch.height);
    if (patch.border_width)
      value.border_width = logical(*patch.border_width);
    if (patch.corner_radius)
      value.corner_radius = logical(*patch.corner_radius);
  };
  apply(style_.base);
  if (!availability.enabled || (cell && !can_change(*repeat_, cell))) {
    value.arrow = theme.palette.disabled;
    apply(style_.disabled);
  } else if (repeat_->armed && repeat_->inside &&
             (!cell || repeat_->direction == cell)) {
    value.fill = theme.palette.accent;
    apply(style_.pressed);
  } else if (hovered_ && (!cell || hovered_ == cell)) {
    value.fill = theme.palette.control_hover;
    apply(style_.hovered);
  }
  if (availability.read_only)
    apply(style_.read_only);
  if (focused_) {
    value.border = theme.palette.focus;
    value.border_width = theme.controls.focus_ring_width;
    apply(style_.focused);
  }
  return value;
}
Size StepperComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto value = presentation(effective_availability(), 0);
  return {value.width, value.height};
}
bool StepperComponent::interaction_affects_layout() const noexcept {
  return style_.hovered.width || style_.hovered.height ||
         style_.pressed.width || style_.pressed.height ||
         style_.focused.width || style_.focused.height;
}
SemanticInfo StepperComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Custom;
  info.name = label_;
  info.numeric_value = effective(*repeat_);
  info.value_range =
      SemanticValueRange{repeat_->minimum, repeat_->maximum, repeat_->step};
  info.focusable = focusable();
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  if (info.enabled && repeat_->minimum < repeat_->maximum) {
    if (info.focusable)
      info.actions = {SemanticAction::Focus};
    if (!info.read_only && repeat_->source.valid()) {
      if (can_change(*repeat_, 1))
        info.actions.push_back(SemanticAction::Increment);
      if (can_change(*repeat_, -1))
        info.actions.push_back(SemanticAction::Decrement);
      info.actions.push_back(SemanticAction::SetValue);
    }
  }
  return info;
}
void StepperComponent::mount(MountContext &context) {
  repeat_->mounted = true;
  repeat_->invalidate = context.invalidator();
  repeat_->invalidate_layout = context.layout_invalidator();
  const std::weak_ptr<StepperRepeat> weak = repeat_;
  subscription_ = repeat_->source.observe([weak](double) {
    const auto current = weak.lock();
    if (current && current->mounted && current->invalidate)
      current->invalidate();
  });
}
void StepperComponent::activate(LifecycleContext &context) {
  repeat_->dispatcher = context.dispatcher();
  repeat_->active = true;
  repeat_->allowed = effective_enabled() && !effective_read_only() &&
                     effective_visibility() == VisibilityMode::Visible;
}
void StepperComponent::deactivate(LifecycleContext &context) {
  repeat_->active = false;
  end_contact(repeat_);
  hovered_ = 0;
  focused_ = false;
  if (interaction_affects_layout())
    context.invalidate_layout();
  else
    context.invalidate();
}
void StepperComponent::unmount(LifecycleContext &) {
  repeat_->mounted = false;
  repeat_->active = false;
  end_contact(repeat_);
  repeat_->invalidate = {};
  repeat_->invalidate_layout = {};
  subscription_.reset();
}
void StepperComponent::focus_changed(bool focused, FocusContext &context) {
  focused_ = focused;
  if (!focused)
    end_contact(repeat_);
  if (interaction_affects_layout())
    context.invalidate_layout();
  else
    context.invalidate();
}
void StepperComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  repeat_->allowed = after.enabled && !after.read_only &&
                     after.visibility == VisibilityMode::Visible;
  if (!repeat_->allowed)
    end_contact(repeat_);
}
void StepperComponent::layout_committed(Rect, Rect current) noexcept {
  if (current.empty())
    end_contact(repeat_);
}
bool StepperComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  const auto a = presentation(before, 0);
  const auto b = presentation(after, 0);
  return a.width != b.width || a.height != b.height;
}
EventResult StepperComponent::input(const InputEvent &event,
                                    InputContext &context) {
  auto repeat = repeat_;
  if (event.type == InputType::PointerLeave) {
    hovered_ = 0;
    repeat->inside = false;
    cancel_timer(repeat);
    invalidate_input(repeat, context, interaction_affects_layout());
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerCancel ||
      (event.type == InputType::KeyDown && event.key == Key::Escape)) {
    end_contact(repeat);
    hovered_ = 0;
    invalidate_input(repeat, context, interaction_affects_layout());
    return EventResult::Handled;
  }
  if (effective_read_only() || !repeat->source.valid()) {
    end_contact(repeat);
    return event.type == InputType::KeyDown ||
                   event.type == InputType::PointerDown ||
                   event.type == InputType::PointerUp
               ? EventResult::Handled
               : EventResult::Ignored;
  }
  if (event.type == InputType::PointerMove) {
    hovered_ = cell_at(context.bounds(), event.position);
    if (repeat->armed) {
      const bool inside = hovered_ == repeat->direction;
      if (inside != repeat->inside) {
        repeat->inside = inside;
        if (inside)
          arm_timer(repeat, DispatcherDuration{0.4});
        else
          cancel_timer(repeat);
      }
    }
    invalidate_input(repeat, context, interaction_affects_layout());
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerUp) {
    if (!repeat->armed)
      return EventResult::Ignored;
    end_contact(repeat);
    invalidate_input(repeat, context, interaction_affects_layout());
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerDown) {
    const bool layout = interaction_affects_layout();
    auto request = repeat->request_editor_focus;
    const int direction = cell_at(context.bounds(), event.position);
    if (!direction || !can_change(*repeat, direction))
      return EventResult::Handled;
    auto release = context.pointer_releaser();
    end_contact(repeat);
    ++repeat->contact_generation;
    repeat->armed = true;
    repeat->inside = true;
    repeat->direction = direction;
    repeat->release = std::move(release);
    repeat->mutation_guard = InputMutationAccess::guard(context);
    hovered_ = direction;
    context.capture_pointer();
    const double next = incremented(*repeat, direction);
    if (direction > 0 ? next < repeat->maximum : next > repeat->minimum)
      arm_timer(repeat, DispatcherDuration{0.4});
    const auto generation = repeat->contact_generation;
    if (request)
      request();
    if (repeat->mounted && repeat->contact_generation == generation &&
        repeat->source.valid())
      publish(repeat, next, layout);
    return EventResult::Handled;
  }
  if (event.type == InputType::KeyDown) {
    int direction{};
    double next{};
    if (event.key == Key::Up || event.key == Key::Right)
      direction = 1;
    else if (event.key == Key::Down || event.key == Key::Left)
      direction = -1;
    else if (event.key == Key::Home)
      next = repeat->minimum;
    else if (event.key == Key::End)
      next = repeat->maximum;
    else
      return EventResult::Ignored;
    end_contact(repeat);
    repeat->mutation_guard = InputMutationAccess::guard(context);
    if (direction)
      next = incremented(*repeat, direction);
    if (repeat->minimum < repeat->maximum) {
      publish(repeat, next, interaction_affects_layout());
    }
    return EventResult::Handled;
  }
  return EventResult::Ignored;
}
void StepperComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  if (bounds.empty())
    return;
  auto &painter = context.painter();
  const auto base = presentation(effective_availability(), 0);
  auto clip = painter.scoped_clip(bounds, base.corner_radius);
  painter.fill_rounded_rect(bounds, base.corner_radius, base.fill);
  for (const int direction : {1, -1}) {
    const auto value = presentation(effective_availability(), direction);
    const Rect cell{bounds.x,
                    bounds.y + (direction > 0 ? 0.0f : bounds.h * 0.5f),
                    bounds.w, bounds.h * 0.5f};
    painter.fill_rounded_rect(cell, 0.0f, value.fill);
    const Point center{cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f};
    const float radius = std::min(cell.w, cell.h) * 0.2f;
    const float apex = center.y - direction * radius;
    const float foot = center.y + direction * radius;
    painter.line({center.x - radius, foot}, {center.x, apex}, 1.5f,
                 value.arrow);
    painter.line({center.x, apex}, {center.x + radius, foot}, 1.5f,
                 value.arrow);
  }
  painter.line({bounds.x, bounds.y + bounds.h * 0.5f},
               {bounds.x + bounds.w, bounds.y + bounds.h * 0.5f},
               base.border_width, base.border);
  painter.stroke_rounded_rect(bounds, base.corner_radius, base.border_width,
                              base.border);
}
} // namespace detail
Stepper::Stepper(Binding<double> value) : value_(std::move(value)) {}
Stepper::Stepper(State<double> &value) : Stepper(value.binding()) {}
Stepper &&Stepper::label(std::string value) && {
  label_ = std::move(value);
  return std::move(*this);
}
Stepper &&Stepper::range(double minimum, double maximum) && {
  validate_range(minimum, maximum);
  minimum_ = minimum;
  maximum_ = maximum;
  return std::move(*this);
}
Stepper &&Stepper::step(double value) && {
  validate_step(value);
  step_ = value;
  return std::move(*this);
}
Stepper &&Stepper::style(StepperStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Spec Stepper::spec() && {
  return {[value = value_, label = std::move(label_), minimum = minimum_,
           maximum = maximum_, step = step_, style = std::move(style_)] {
            return std::make_unique<detail::StepperComponent>(
                value, label, minimum, maximum, step, style);
          },
          {}};
}
} // namespace ui
