#include <nativeui/animation.hpp>
#include <nativeui/spinner.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_size(double value) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument("Spinner size must be finite and nonnegative");
}
void validate_style(const SpinnerStyle &value) {
  if (!std::isfinite(value.thickness_ratio) || value.thickness_ratio <= 0.0 ||
      value.thickness_ratio > 0.5)
    throw std::invalid_argument(
        "Spinner thickness ratio must be within (0, 0.5]");
  if (value.size)
    validate_size(*value.size);
}
float logical_size(double value) noexcept {
  return static_cast<float>(
      std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}
} // namespace
namespace detail {
struct SpinnerMotion {
  explicit SpinnerMotion(std::optional<Binding<bool>> value, bool reduced)
      : source(std::move(value)), reduced_motion(reduced) {}
  std::optional<Binding<bool>> source;
  Dispatcher dispatcher;
  std::unique_ptr<AnimationContext> animations;
  AnimationInvalidationTarget target;
  AnimationHandle handle;
  std::uint64_t generation{};
  float phase{};
  bool mounted{};
  bool active{};
  bool allowed{true};
  bool bounds_nonempty{};
  bool reduced_motion{};
  bool moving{};
};

namespace {
void stop_motion(const std::shared_ptr<SpinnerMotion> &motion) noexcept {
  if (!motion)
    return;
  ++motion->generation;
  if (motion->animations)
    (void)motion->animations->cancel(motion->handle);
  motion->handle = {};
  motion->moving = false;
  motion->phase = 0.0f;
}

bool can_animate(const SpinnerMotion &motion) noexcept {
  return motion.mounted && motion.active && motion.allowed &&
         motion.bounds_nonempty && !motion.reduced_motion &&
         (!motion.source || (motion.source->valid() && motion.source->get())) &&
         motion.dispatcher.valid();
}

void start_motion(const std::shared_ptr<SpinnerMotion> &motion) noexcept {
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
    const std::weak_ptr<SpinnerMotion> weak = motion;
    motion->phase = 0.0f;
    motion->handle = motion->animations->start_tween(
        0.0f, 1.0f, DispatcherDuration{0.9}, Easing::Linear,
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
      motion->phase = 0.0f;
  } catch (...) {
    // A failed wake/allocation leaves no published animation entry. A future
    // active/availability/committed geometry transition can retry; rendering
    // remains free of scheduling effects and uses static rays.
    stop_motion(motion);
  }
}

bool nonempty_bounds(Rect bounds) noexcept {
  return bounds.w > 0.0f && bounds.h > 0.0f && std::isfinite(bounds.w) &&
         std::isfinite(bounds.h);
}
} // namespace

SpinnerComponent::SpinnerComponent(std::string label,
                                   std::optional<Binding<bool>> active,
                                   std::optional<double> size,
                                   bool reduced_motion, SpinnerStyle style)
    : label_(std::move(label)), size_(size), style_(std::move(style)),
      motion_(
          std::make_shared<SpinnerMotion>(std::move(active), reduced_motion)) {
  if (size_)
    validate_size(*size_);
  validate_style(style_);
}
double SpinnerComponent::diameter() const noexcept {
  return size_.value_or(style_.size.value_or(18.0));
}
Size SpinnerComponent::measure(const std::vector<ChildMetrics> &) const {
  const float size = logical_size(diameter());
  return {size, size};
}
Size SpinnerComponent::minimum_size(
    const std::vector<ChildMetrics> &children) const {
  return measure(children);
}
SemanticInfo SpinnerComponent::semantics() const {
  SemanticInfo info;
  if (motion_->source && (!motion_->source->valid() || !motion_->source->get()))
    return info;
  info.role = SemanticRole::ProgressBar;
  info.name = label_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  return info;
}
void SpinnerComponent::mount(MountContext &context) {
  motion_->mounted = true;
  motion_->target = AnimationInvalidationTarget{context.invalidator(),
                                                context.layout_invalidator()};
  if (motion_->source) {
    const std::weak_ptr<SpinnerMotion> weak = motion_;
    auto invalidate = context.invalidator();
    subscription_ = motion_->source->observe(
        [weak, invalidate = std::move(invalidate)](bool) {
          const auto current = weak.lock();
          if (!current || !current->mounted)
            return;
          start_motion(current);
          invalidate();
        });
  }
}
void SpinnerComponent::activate(LifecycleContext &context) {
  stop_motion(motion_);
  motion_->animations.reset();
  motion_->dispatcher = context.dispatcher();
  motion_->active = true;
  motion_->allowed =
      effective_visibility() == VisibilityMode::Visible && effective_enabled();
  motion_->bounds_nonempty =
      diameter() > 0.0 && nonempty_bounds(context.bounds());
  start_motion(motion_);
}
void SpinnerComponent::deactivate(LifecycleContext &context) {
  const bool moving = motion_->moving;
  motion_->active = false;
  stop_motion(motion_);
  if (moving)
    context.invalidate();
}
void SpinnerComponent::unmount(LifecycleContext &) {
  motion_->mounted = false;
  motion_->active = false;
  stop_motion(motion_);
  motion_->animations.reset();
  motion_->target = {};
  subscription_.reset();
}
void SpinnerComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  motion_->allowed =
      after.visibility == VisibilityMode::Visible && after.enabled;
  start_motion(motion_);
}
void SpinnerComponent::layout_committed(Rect previous, Rect current) noexcept {
  if (previous.x == current.x && previous.y == current.y &&
      previous.w == current.w && previous.h == current.h)
    return;
  motion_->bounds_nonempty = diameter() > 0.0 && nonempty_bounds(current);
  start_motion(motion_);
}
EventResult SpinnerComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void SpinnerComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const float size = std::min({bounds.w, bounds.h, logical_size(diameter())});
  if (size <= 0.0f || !nonempty_bounds(bounds) ||
      (motion_->source &&
       (!motion_->source->valid() || !motion_->source->get())))
    return;
  auto color = style_.color.value_or(current_theme().palette.muted_text);
  if (!effective_enabled())
    color = current_theme().palette.disabled;
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  const Point center{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f};
  const float width = size * static_cast<float>(style_.thickness_ratio);
  const int lead = static_cast<int>(std::floor(motion_->phase * 12.0f)) % 12;
  for (int i = 0; i < 12; ++i) {
    const int distance = (lead + 12 - i) % 12;
    auto ray_color = color;
    ray_color.a *= 1.0f - static_cast<float>(distance) * (0.85f / 11.0f);
    const float angle =
        static_cast<float>(i) * (2.0f * kPi / 12.0f) - kPi * 0.5f;
    const Point inner{center.x + std::cos(angle) * size * 0.22f,
                      center.y + std::sin(angle) * size * 0.22f};
    const Point outer{center.x + std::cos(angle) * size * 0.46f,
                      center.y + std::sin(angle) * size * 0.46f};
    painter.line(inner, outer, width, ray_color);
  }
}
} // namespace detail
Spinner::Spinner(std::string label) : label_(std::move(label)) {}
Spinner &&Spinner::active(Binding<bool> value) && {
  active_ = std::move(value);
  return std::move(*this);
}
Spinner &&Spinner::active(State<bool> &value) && {
  return std::move(*this).active(value.binding());
}
Spinner &&Spinner::size(double value) && {
  validate_size(value);
  size_ = value;
  return std::move(*this);
}
Spinner &&Spinner::reduced_motion(bool value) && {
  reduced_motion_ = value;
  return std::move(*this);
}
Spinner &&Spinner::style(SpinnerStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Spec Spinner::spec() && {
  return {[label = std::move(label_), active = std::move(active_), size = size_,
           reduced_motion = reduced_motion_, style = std::move(style_)] {
            return std::make_unique<detail::SpinnerComponent>(
                label, active, size, reduced_motion, style);
          },
          {}};
}
} // namespace ui
