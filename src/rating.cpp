#include <nativeui/rating.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_step(double value) {
  if (value != 1.0 && value != 0.5)
    throw std::invalid_argument("Rating step must be 1 or 0.5");
}
void validate_style(const RatingStyle &value) {
  for (const double metric : {value.star_size, value.gap, value.outline_width})
    if (!std::isfinite(metric) || metric < 0.0)
      throw std::invalid_argument(
          "Rating metrics must be finite and nonnegative");
}
float logical(long double value) noexcept {
  return static_cast<float>(std::min(
      value, static_cast<long double>(std::numeric_limits<float>::max())));
}
Path star_path() {
  Path path;
  for (int i = 0; i < 10; ++i) {
    const float angle = static_cast<float>(i) * (kPi / 5.0f) - kPi * 0.5f;
    const float radius = i % 2 ? 0.21f : 0.48f;
    const Point point{0.5f + std::cos(angle) * radius,
                      0.5f + std::sin(angle) * radius};
    if (!i)
      path.move_to(point);
    else
      path.line_to(point);
  }
  path.close();
  return path;
}
} // namespace
namespace detail {
RatingComponent::RatingComponent(std::string label, Binding<double> value,
                                 std::size_t stars, double step, bool clearable,
                                 RatingStyle style)
    : label_(std::move(label)), value_(std::move(value)), stars_(stars),
      step_(step), clearable_(clearable), style_(std::move(style)),
      star_path_(star_path()) {
  validate_step(step_);
  validate_style(style_);
}
bool RatingComponent::focusable() const noexcept { return stars_ != 0; }
bool RatingComponent::cancel_capture_on_read_only() const noexcept {
  return true;
}
double RatingComponent::effective() const noexcept {
  const double value = value_.get();
  return std::isfinite(value)
             ? std::clamp(value, 0.0, static_cast<double>(stars_))
             : 0.0;
}
Size RatingComponent::measure(const std::vector<ChildMetrics> &) const {
  if (!stars_)
    return {};
  const long double width =
      static_cast<long double>(stars_) * style_.star_size +
      static_cast<long double>(stars_ - 1) * style_.gap;
  return {logical(width),
          logical(static_cast<long double>(style_.star_size) +
                  current_theme().controls.focus_ring_width * 2.0L)};
}
SemanticInfo RatingComponent::semantics() const {
  SemanticInfo info;
  if (!stars_)
    return info;
  info.role = SemanticRole::Slider;
  info.name = label_;
  info.numeric_value = effective();
  info.value_range =
      SemanticValueRange{0.0, static_cast<double>(stars_), step_};
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  if (info.enabled) {
    info.actions = {SemanticAction::Focus};
    if (!info.read_only && value_.valid())
      info.actions.insert(info.actions.end(),
                          {SemanticAction::Increment, SemanticAction::Decrement,
                           SemanticAction::SetValue});
  }
  return info;
}
void RatingComponent::mount(MountContext &context) {
  auto invalidate = context.invalidator();
  subscription_ = value_.observe(
      [invalidate = std::move(invalidate)](double) { invalidate(); });
}
void RatingComponent::cancel_press() noexcept {
  armed_ = false;
  preview_.reset();
  auto release = std::move(release_pointer_);
  if (release) {
    try {
      release();
    } catch (...) {
    }
  }
}
void RatingComponent::unmount(LifecycleContext &) {
  cancel_press();
  subscription_.reset();
}
void RatingComponent::deactivate(LifecycleContext &context) {
  cancel_press();
  focused_ = false;
  context.invalidate();
}
void RatingComponent::focus_changed(bool focused, FocusContext &context) {
  focused_ = focused;
  if (!focused)
    cancel_press();
  context.invalidate();
}
void RatingComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  if (!after.enabled || after.read_only ||
      after.visibility != VisibilityMode::Visible)
    cancel_press();
}
void RatingComponent::layout_committed(Rect, Rect current) noexcept {
  if (current.empty())
    cancel_press();
}
std::optional<double> RatingComponent::value_at(Rect bounds,
                                                Point point) const noexcept {
  if (!stars_ || style_.star_size <= 0.0 || bounds.empty() ||
      !bounds.contains(point))
    return {};
  const long double pitch =
      static_cast<long double>(style_.star_size) + style_.gap;
  const long double x = static_cast<long double>(point.x) - bounds.x;
  const long double index = std::floor(x / pitch);
  if (index < 0.0L || index >= static_cast<long double>(stars_))
    return {};
  const long double offset = x - index * pitch;
  if (offset > style_.star_size)
    return {};
  return static_cast<double>(index) +
         (step_ == 0.5 && offset < style_.star_size * 0.5L ? 0.5 : 1.0);
}
EventResult RatingComponent::input(const InputEvent &event,
                                   InputContext &context) {
  if (event.type == InputType::PointerLeave) {
    preview_.reset();
    context.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerCancel ||
      (event.type == InputType::KeyDown && event.key == Key::Escape)) {
    cancel_press();
    context.invalidate();
    return EventResult::Handled;
  }
  if (effective_read_only() || !value_.valid() || !stars_) {
    cancel_press();
    return event.type == InputType::KeyDown ||
                   event.type == InputType::PointerDown ||
                   event.type == InputType::PointerUp
               ? EventResult::Handled
               : EventResult::Ignored;
  }
  if (event.type == InputType::PointerMove) {
    preview_ = value_at(context.bounds(), event.position);
    context.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerDown) {
    const auto target = value_at(context.bounds(), event.position);
    if (!target)
      return EventResult::Handled;
    auto release = context.pointer_releaser();
    cancel_press();
    release_pointer_ = std::move(release);
    armed_ = true;
    preview_ = target;
    context.capture_pointer();
    context.invalidate();
    return EventResult::Handled;
  }
  if (event.type == InputType::PointerUp) {
    if (!armed_)
      return EventResult::Ignored;
    const auto target = value_at(context.bounds(), event.position);
    const double next = target && clearable_ && *target == effective()
                            ? 0.0
                            : target.value_or(0.0);
    auto source = value_;
    cancel_press();
    context.invalidate();
    if (target && source.valid() && InputMutationAccess::allowed(context))
      source.set(next);
    return EventResult::Handled;
  }
  if (event.type == InputType::KeyDown) {
    double next = effective();
    if (event.key == Key::Right || event.key == Key::Up)
      next += step_;
    else if (event.key == Key::Left || event.key == Key::Down)
      next -= step_;
    else if (event.key == Key::Home)
      next = 0.0;
    else if (event.key == Key::End)
      next = static_cast<double>(stars_);
    else
      return EventResult::Ignored;
    next = std::clamp(std::round(next / step_) * step_, 0.0,
                      static_cast<double>(stars_));
    auto source = value_;
    cancel_press();
    context.invalidate();
    if (source.valid() && InputMutationAccess::allowed(context))
      source.set(next);
    return EventResult::Handled;
  }
  return EventResult::Ignored;
}
void RatingComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const float size = logical(style_.star_size);
  if (!stars_ || size <= 0.0f || bounds.empty())
    return;
  const auto &theme = current_theme();
  const bool mutable_value =
      effective_enabled() && !effective_read_only() && value_.valid();
  const double value = mutable_value && preview_ ? *preview_ : effective();
  const auto empty = style_.empty.value_or(theme.palette.control_background);
  const auto full = !effective_enabled()
                        ? style_.disabled.value_or(theme.palette.disabled)
                    : preview_ && mutable_value
                        ? style_.preview.value_or(theme.palette.control_hover)
                        : style_.filled.value_or(theme.palette.accent);
  const auto outline = style_.outline.value_or(theme.palette.border);
  const double pitch = static_cast<double>(size) + logical(style_.gap);
  const float y = bounds.y + (bounds.h - size) * 0.5f;
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  for (std::size_t i = 0; i < stars_; ++i) {
    const double x =
        static_cast<double>(bounds.x) + static_cast<double>(i) * pitch;
    if (x >= static_cast<double>(bounds.x) + bounds.w)
      break;
    auto transform = painter.scoped_state();
    painter.translate(static_cast<float>(x), y);
    painter.scale(size, size);
    painter.fill_path(star_path_, empty);
    const float fraction = static_cast<float>(
        std::clamp(value - static_cast<double>(i), 0.0, 1.0));
    if (fraction > 0.0f) {
      auto fraction_clip = painter.scoped_clip({0.0f, 0.0f, fraction, 1.0f});
      painter.fill_path(star_path_, full);
    }
    if (style_.outline_width > 0.0)
      painter.stroke_path(
          star_path_, outline,
          StrokeStyle{.width = logical(style_.outline_width) / size});
  }
  if (context.focused()) {
    const auto measured = measure({});
    const Rect group{bounds.x, bounds.y, std::min(bounds.w, measured.w),
                     bounds.h};
    painter.stroke_rounded_rect(
        group, theme.radii.sm, theme.controls.focus_ring_width,
        style_.focus_ring.value_or(theme.palette.focus));
  }
}
} // namespace detail
Rating::Rating(std::string label, Binding<double> value, std::size_t stars)
    : label_(std::move(label)), value_(std::move(value)), stars_(stars) {}
Rating::Rating(std::string label, State<double> &value, std::size_t stars)
    : Rating(std::move(label), value.binding(), stars) {}
Rating &&Rating::step(double value) && {
  validate_step(value);
  step_ = value;
  return std::move(*this);
}
Rating &&Rating::clearable(bool value) && {
  clearable_ = value;
  return std::move(*this);
}
Rating &&Rating::style(RatingStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Spec Rating::spec() && {
  return {[label = std::move(label_), value = value_, stars = stars_,
           step = step_, clearable = clearable_, style = std::move(style_)] {
            return std::make_unique<detail::RatingComponent>(
                label, value, stars, step, clearable, style);
          },
          {}};
}
} // namespace ui
