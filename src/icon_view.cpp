#include "detail/widget_svg_tint.hpp"
#include <nativeui/icon_view.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_height(double value) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument(
        "IconView height must be finite and nonnegative");
}
Color validated_color(Color value) {
  if (!std::isfinite(value.r) || !std::isfinite(value.g) ||
      !std::isfinite(value.b) || !std::isfinite(value.a))
    throw std::invalid_argument("IconView color components must be finite");
  return {std::clamp(value.r, 0.0f, 1.0f), std::clamp(value.g, 0.0f, 1.0f),
          std::clamp(value.b, 0.0f, 1.0f), std::clamp(value.a, 0.0f, 1.0f)};
}
double ratio(const SvgIcon &value) noexcept {
  const auto intrinsic = value.intrinsic_size();
  return value.valid() && intrinsic.w > 0.0f && intrinsic.h > 0.0f
             ? static_cast<double>(intrinsic.w) / intrinsic.h
             : 1.0;
}
float logical(double value) noexcept {
  return static_cast<float>(std::clamp(
      value, 0.0, static_cast<double>(std::numeric_limits<float>::max())));
}
} // namespace
namespace detail {
IconViewComponent::IconViewComponent(SvgIcon source,
                                     std::optional<Binding<SvgIcon>> binding,
                                     std::optional<double> height,
                                     std::optional<Color> color,
                                     bool monochrome, std::string alt,
                                     bool decorative)
    : source_(std::move(source)), binding_(std::move(binding)), height_(height),
      color_(color), monochrome_(monochrome), alt_(std::move(alt)),
      decorative_(decorative) {
  if (height_)
    validate_height(*height_);
  if (color_)
    color_ = validated_color(*color_);
}
SvgIcon IconViewComponent::current_source() const {
  return binding_ ? binding_->get() : source_;
}
double IconViewComponent::height() const noexcept {
  return height_.value_or(
      std::max(0.0f, current_theme().typography.control_size));
}
Size IconViewComponent::measure(const std::vector<ChildMetrics> &) const {
  const double chosen = height();
  return {logical(chosen * ratio(current_source())), logical(chosen)};
}
SemanticInfo IconViewComponent::semantics() const {
  SemanticInfo info;
  if (!decorative_) {
    info.role = SemanticRole::Image;
    info.name = alt_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
  }
  return info;
}
void IconViewComponent::mount(MountContext &context) {
  if (!binding_)
    return;
  auto previous = std::make_shared<double>(ratio(current_source()));
  auto invalidate = context.invalidator();
  auto invalidate_layout = context.layout_invalidator();
  subscription_ = binding_->observe(
      [previous = std::move(previous), invalidate = std::move(invalidate),
       invalidate_layout =
           std::move(invalidate_layout)](const SvgIcon &source) {
        const double next = ratio(source);
        const bool changed = *previous != next;
        *previous = next;
        if (changed)
          invalidate_layout();
        else
          invalidate();
      });
}
void IconViewComponent::unmount(LifecycleContext &) { subscription_.reset(); }
EventResult IconViewComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void IconViewComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto source = current_source();
  const double chosen = height();
  if (!source.valid() || chosen <= 0.0 || bounds.empty())
    return;
  double h = std::min(static_cast<double>(bounds.h), chosen);
  double w = h * ratio(source);
  if (w > bounds.w) {
    h *= static_cast<double>(bounds.w) / w;
    w = bounds.w;
  }
  if (!(w > 0.0 && h > 0.0) || !std::isfinite(w) || !std::isfinite(h))
    return;
  const Rect destination{bounds.x + (bounds.w - static_cast<float>(w)) * 0.5f,
                         bounds.y + (bounds.h - static_cast<float>(h)) * 0.5f,
                         static_cast<float>(w), static_cast<float>(h)};
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  if (monochrome_) {
    const auto color =
        color_.value_or(effective_enabled() ? current_theme().palette.text
                                            : current_theme().palette.disabled);
    draw_svg_monochrome(painter, source, destination, color);
  } else
    draw_svg(painter, source, destination);
}
} // namespace detail
IconView::IconView(SvgIcon value) : source_(std::move(value)) {}
IconView::IconView(Binding<SvgIcon> value) : binding_(std::move(value)) {}
IconView::IconView(State<SvgIcon> &value) : IconView(value.binding()) {}
IconView &&IconView::size(double height) && {
  validate_height(height);
  height_ = height;
  return std::move(*this);
}
IconView &&IconView::color(Color value) && {
  color_ = validated_color(value);
  return std::move(*this);
}
IconView &&IconView::monochrome(bool value) && {
  monochrome_ = value;
  return std::move(*this);
}
IconView &&IconView::alt(std::string value) && {
  alt_ = std::move(value);
  decorative_ = false;
  return std::move(*this);
}
IconView &&IconView::decorative(bool value) && {
  decorative_ = value;
  return std::move(*this);
}
Spec IconView::spec() && {
  return {[source = std::move(source_), binding = binding_, height = height_,
           color = color_, monochrome = monochrome_, alt = std::move(alt_),
           decorative = decorative_] {
            return std::make_unique<detail::IconViewComponent>(
                source, binding, height, color, monochrome, alt, decorative);
          },
          {}};
}
} // namespace ui
