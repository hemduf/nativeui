#include <nativeui/animation.hpp>
#include <nativeui/progress_bar.hpp>

#include <cstdint>

namespace ui {

namespace detail {

struct ProgressMotion {
  explicit ProgressMotion(Binding<float> value, bool reduced)
      : source(std::move(value)), reduced_motion(reduced) {}
  Binding<float> source;
  Dispatcher dispatcher;
  std::unique_ptr<AnimationContext> animations;
  AnimationInvalidationTarget target;
  AnimationHandle handle;
  std::uint64_t generation{};
  float phase{0.5f};
  bool mounted{};
  bool active{};
  bool allowed{true};
  bool bounds_nonempty{};
  bool reduced_motion{};
  bool moving{};
};

namespace {
void stop_motion(const std::shared_ptr<ProgressMotion> &motion) noexcept {
  if (!motion)
    return;
  ++motion->generation;
  if (motion->animations)
    (void)motion->animations->cancel(motion->handle);
  motion->handle = {};
  motion->moving = false;
  motion->phase = 0.5f;
}

bool can_animate(const ProgressMotion &motion) noexcept {
  return motion.mounted && motion.active && motion.allowed &&
         motion.bounds_nonempty && !motion.reduced_motion &&
         motion.source.valid() && motion.dispatcher.valid();
}

void start_motion(const std::shared_ptr<ProgressMotion> &motion) noexcept {
  if (!motion || !can_animate(*motion)) {
    stop_motion(motion);
    return;
  }
  if (motion->moving)
    return;
  try {
    if (!motion->animations)
      motion->animations =
          std::make_unique<AnimationContext>(motion->dispatcher);
    const auto generation = ++motion->generation;
    const std::weak_ptr<ProgressMotion> weak = motion;
    motion->phase = 0.0f;
    motion->handle = motion->animations->start_tween(
        0.0f, 1.0f, DispatcherDuration{1.4}, Easing::Linear,
        AnimationInvalidation::Paint, motion->target,
        [weak, generation](float phase) {
          const auto current = weak.lock();
          if (!current || current->generation != generation)
            return;
          if (!can_animate(*current)) {
            stop_motion(current);
            return;
          }
          current->phase = phase;
        },
        [weak, generation] {
          const auto current = weak.lock();
          if (!current || current->generation != generation)
            return;
          current->handle = {};
          current->moving = false;
          start_motion(current);
        });
    motion->moving = motion->handle.valid();
    if (!motion->moving)
      motion->phase = 0.5f;
  } catch (...) {
    // A failed wake/allocation leaves no published animation entry. A future
    // committed geometry/availability transition can retry; rendering remains
    // free of scheduling effects and uses the centered static segment.
    stop_motion(motion);
  }
}

bool nonempty_bounds(Rect bounds) noexcept {
  return bounds.w > 0.0f && bounds.h > 0.0f && std::isfinite(bounds.w) &&
         std::isfinite(bounds.h);
}

Rect activity_rect(Rect bounds, float phase, ProgressOrientation orientation,
                   bool reversed) noexcept {
  constexpr float segment = 0.3f;
  const float offset = phase * (1.0f + segment) - segment;
  Rect fill;
  if (orientation == ProgressOrientation::Horizontal) {
    const float origin = reversed ? 1.0f - offset - segment : offset;
    fill = {bounds.x + bounds.w * origin, bounds.y, bounds.w * segment,
            bounds.h};
  } else {
    const float origin = reversed ? offset : 1.0f - offset - segment;
    fill = {bounds.x, bounds.y + bounds.h * origin, bounds.w,
            bounds.h * segment};
  }
  return intersect(bounds, fill);
}
} // namespace

BoundedDisplayDomain::BoundedDisplayDomain(float minimum, float maximum)
    : minimum_(minimum), maximum_(maximum) {
  if (!std::isfinite(minimum_) || !std::isfinite(maximum_) ||
      minimum_ >= maximum_) {
    throw std::invalid_argument(
        "Progress/Meter range must be finite with minimum < maximum");
  }
}

float BoundedDisplayDomain::minimum() const noexcept { return minimum_; }

float BoundedDisplayDomain::maximum() const noexcept { return maximum_; }

float BoundedDisplayDomain::effective(float value) const noexcept {
  if (!std::isfinite(value))
    return minimum_;
  return std::clamp(value, minimum_, maximum_);
}

float BoundedDisplayDomain::fraction(float value) const noexcept {
  // The endpoints are finite binary32 values, but maximum-minimum can
  // still overflow binary32 for a valid wide range. Normalize in double
  // so every accepted finite range produces finite [0, 1] geometry.
  const double minimum = static_cast<double>(minimum_);
  const double span = static_cast<double>(maximum_) - minimum;
  const double normalized =
      (static_cast<double>(effective(value)) - minimum) / span;
  return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
}

Rect BoundedDisplayDomain::fill_rect(
    Rect bounds, float value, ProgressOrientation orientation) const noexcept {
  const float normalized = fraction(value);
  if (orientation == ProgressOrientation::Vertical) {
    const float height = bounds.h * normalized;
    return Rect{bounds.x, bounds.y + bounds.h - height, bounds.w, height};
  }
  return Rect{bounds.x, bounds.y, bounds.w * normalized, bounds.h};
}

BoundedDisplayComponent::BoundedDisplayComponent(
    State<float> &state, float minimum, float maximum,
    ProgressOrientation orientation, Formatter formatter,
    ProgressBarStyle style)
    : BoundedDisplayComponent(state.binding(), minimum, maximum, orientation,
                              std::move(formatter), std::move(style)) {}

BoundedDisplayComponent::BoundedDisplayComponent(
    State<float> &state, float minimum, float maximum,
    ProgressOrientation orientation, Formatter formatter, MeterStyle style)
    : BoundedDisplayComponent(state.binding(), minimum, maximum, orientation,
                              std::move(formatter), std::move(style)) {}

BoundedDisplayComponent::BoundedDisplayComponent(
    Binding<float> state, float minimum, float maximum,
    ProgressOrientation orientation, Formatter formatter,
    ProgressBarStyle style, bool reversed, bool indeterminate,
    bool reduced_motion)
    : state_(std::move(state)), domain_(minimum, maximum),
      orientation_(orientation), formatter_(std::move(formatter)),
      progress_style_(std::move(style)), reversed_(reversed),
      indeterminate_(indeterminate) {
  if (indeterminate_)
    motion_ = std::make_shared<ProgressMotion>(state_, reduced_motion);
}

BoundedDisplayComponent::BoundedDisplayComponent(
    Binding<float> state, float minimum, float maximum,
    ProgressOrientation orientation, Formatter formatter, MeterStyle style,
    std::optional<MeterLevels> levels, Color warning, Color critical)
    : state_(std::move(state)), domain_(minimum, maximum),
      orientation_(orientation), formatter_(std::move(formatter)),
      meter_style_(std::move(style)), meter_(true), levels_(levels),
      warning_(warning), critical_(critical) {
  if (levels_ &&
      (!std::isfinite(levels_->warning) || !std::isfinite(levels_->critical) ||
       levels_->warning < minimum || levels_->warning > maximum ||
       levels_->critical < minimum || levels_->critical > maximum))
    throw std::invalid_argument(
        "Meter levels must be finite and within its range");
}

SemanticInfo BoundedDisplayComponent::semantics() const {
  SemanticInfo info;
  info.role = meter_ ? SemanticRole::Meter : SemanticRole::ProgressBar;
  const float effective = domain_.effective(state_.get());
  if (indeterminate_)
    info.description = "progression indéterminée";
  else {
    info.numeric_value = effective;
    info.value_range =
        SemanticValueRange{domain_.minimum(), domain_.maximum(), 0.0};
  }
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  if (meter_)
    info.description = meter_level_description(effective, levels_);
  return info;
}

bool BoundedDisplayComponent::focusable() const noexcept { return false; }

Size BoundedDisplayComponent::measure(const std::vector<ChildMetrics> &) const {
  return preferred_size(resolved_style());
}

void BoundedDisplayComponent::mount(MountContext &context) {
  if (motion_) {
    motion_->mounted = true;
    motion_->target = AnimationInvalidationTarget{context.invalidator(),
                                                  context.layout_invalidator()};
  }
  auto invalidate = context.invalidator();
  subscription_ = state_.observe(
      [invalidate = std::move(invalidate)](const float &) { invalidate(); });
}

void BoundedDisplayComponent::unmount(LifecycleContext &) {
  if (motion_) {
    motion_->mounted = false;
    motion_->active = false;
    stop_motion(motion_);
    motion_->animations.reset();
    motion_->target = {};
  }
  subscription_.reset();
}

void BoundedDisplayComponent::activate(LifecycleContext &context) {
  if (!motion_)
    return;
  stop_motion(motion_);
  motion_->animations.reset();
  motion_->dispatcher = context.dispatcher();
  motion_->active = true;
  motion_->allowed =
      effective_visibility() == VisibilityMode::Visible && effective_enabled();
  motion_->bounds_nonempty = nonempty_bounds(context.bounds());
  start_motion(motion_);
}

void BoundedDisplayComponent::deactivate(LifecycleContext &context) {
  if (!motion_)
    return;
  const bool moving = motion_->moving;
  motion_->active = false;
  stop_motion(motion_);
  if (moving)
    context.invalidate();
}

void BoundedDisplayComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  if (!motion_)
    return;
  motion_->allowed =
      after.visibility == VisibilityMode::Visible && after.enabled;
  start_motion(motion_);
}

void BoundedDisplayComponent::layout_committed(Rect previous,
                                               Rect current) noexcept {
  if (!motion_)
    return;
  if (previous.x == current.x && previous.y == current.y &&
      previous.w == current.w && previous.h == current.h)
    return;
  motion_->bounds_nonempty = nonempty_bounds(current);
  start_motion(motion_);
}

EventResult BoundedDisplayComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}

void BoundedDisplayComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto resolved = resolved_style();
  auto &painter = context.painter();
  const float effective = domain_.effective(state_.get());
  auto fill = domain_.fill_rect(bounds, effective, orientation_);
  if (motion_) {
    fill = activity_rect(bounds, motion_->phase, orientation_, reversed_);
  } else if (reversed_) {
    if (orientation_ == ProgressOrientation::Horizontal)
      fill.x = bounds.x + bounds.w - fill.w;
    else
      fill.y = bounds.y;
  }
  const Color fill_color = meter_ && effective_enabled()
                               ? meter_fill_color(effective, resolved.fill,
                                                  levels_, warning_, critical_)
                               : resolved.fill;

  painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.track);
  if (fill.w > 0.0f && fill.h > 0.0f) {
    painter.fill_rounded_rect(fill, resolved.fill_corner_radius, fill_color);
  }
  painter.stroke_rounded_rect(bounds, resolved.corner_radius,
                              resolved.border_width, resolved.border);

  auto formatter = formatter_;
  if (formatter && !indeterminate_) {
    const auto text = formatter(effective);
    painter.text(Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
                 text, resolved.text_size, resolved.text, TextAlign::Center);
  }
}

bool BoundedDisplayComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  const auto before_size = preferred_size(resolved_style(before));
  const auto after_size = preferred_size(resolved_style(after));
  return before_size.w != after_size.w || before_size.h != after_size.h;
}

Size BoundedDisplayComponent::preferred_size(
    const ResolvedProgressStyle &resolved) const noexcept {
  if (orientation_ == ProgressOrientation::Vertical) {
    return formatter_ ? resolved.vertical_formatted_size
                      : resolved.vertical_size;
  }
  return formatter_ ? resolved.horizontal_formatted_size
                    : resolved.horizontal_size;
}

VisualState BoundedDisplayComponent::visual_state(
    const ComponentAvailability &availability) noexcept {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
  };
}

VisualState BoundedDisplayComponent::current_visual_state() const noexcept {
  return visual_state(effective_availability());
}

ResolvedProgressStyle BoundedDisplayComponent::resolved_style(
    const ComponentAvailability &availability) const {
  if (meter_) {
    return resolve_meter_style(default_meter_style(current_theme()),
                               meter_style_, visual_state(availability));
  }
  return resolve_progress_bar_style(default_progress_bar_style(current_theme()),
                                    progress_style_,
                                    visual_state(availability));
}

ResolvedProgressStyle BoundedDisplayComponent::resolved_style() const {
  return resolved_style(effective_availability());
}
} // namespace detail

ProgressBar::ProgressBar(State<float> &state, float minimum, float maximum)
    : ProgressBar(state.binding(), minimum, maximum) {}

ProgressBar::ProgressBar(Binding<float> state, float minimum, float maximum)
    : state_(std::move(state)), minimum_(minimum), maximum_(maximum) {}

ProgressBar &&ProgressBar::indeterminate(bool value) && {
  indeterminate_ = value;
  return std::move(*this);
}

ProgressBar &&ProgressBar::reduced_motion(bool value) && {
  reduced_motion_ = value;
  return std::move(*this);
}

ProgressBar &&ProgressBar::reversed(bool value) && {
  reversed_ = value;
  return std::move(*this);
}

ProgressBar &&ProgressBar::orientation(ProgressOrientation value) && {
  orientation_ = value;
  return std::move(*this);
}

ProgressBar &&ProgressBar::formatter(Formatter value) && {
  formatter_ = std::move(value);
  return std::move(*this);
}

ProgressBar &&ProgressBar::style(ProgressBarStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec ProgressBar::spec() && {
  auto state = std::move(state_);
  const float minimum = minimum_;
  const float maximum = maximum_;
  const auto orientation = orientation_;
  const bool reversed = reversed_;
  const bool indeterminate = indeterminate_;
  const bool reduced_motion = reduced_motion_;
  auto formatter = std::move(formatter_);
  auto style = std::move(style_);
  return Spec{[state = std::move(state), minimum, maximum, orientation,
               reversed, indeterminate, reduced_motion,
               formatter = std::move(formatter),
               style = std::move(style)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    std::move(state), minimum, maximum, orientation,
                    std::move(formatter), std::move(style), reversed,
                    indeterminate, reduced_motion);
              },
              {}};
}

} // namespace ui
