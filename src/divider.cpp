#include <nativeui/divider.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
void validate_thickness(double value) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument(
        "Divider thickness must be finite and nonnegative");
}
float logical_thickness(double value) noexcept {
  return static_cast<float>(
      std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}
} // namespace

namespace detail {
DividerComponent::DividerComponent(DividerOrientation orientation,
                                   double thickness, std::optional<Color> color)
    : orientation_(orientation), thickness_(thickness), color_(color) {
  validate_thickness(thickness);
}
Size DividerComponent::measure(const std::vector<ChildMetrics> &) const {
  const float thickness = logical_thickness(thickness_);
  return orientation_ == DividerOrientation::Horizontal ? Size{0.0f, thickness}
                                                        : Size{thickness, 0.0f};
}
Size DividerComponent::minimum_size(
    const std::vector<ChildMetrics> &children) const {
  return measure(children);
}
EventResult DividerComponent::input(const InputEvent &, InputContext &) {
  return EventResult::Ignored;
}
void DividerComponent::paint(PaintContext &context) const {
  auto bounds = context.bounds();
  if (bounds.empty() || thickness_ == 0.0)
    return;
  if (orientation_ == DividerOrientation::Horizontal)
    bounds.h = std::min(bounds.h, logical_thickness(thickness_));
  else
    bounds.w = std::min(bounds.w, logical_thickness(thickness_));
  context.painter().fill_rounded_rect(
      bounds, 0.0f, color_.value_or(current_theme().palette.border));
}
} // namespace detail

Divider::Divider(DividerOrientation value) : orientation_(value) {}
Divider &&Divider::thickness(double value) && {
  validate_thickness(value);
  thickness_ = value;
  return std::move(*this);
}
Divider &&Divider::color(Color value) && {
  color_ = value;
  return std::move(*this);
}
Spec Divider::spec() && {
  return {[orientation = orientation_, thickness = thickness_, color = color_] {
            return std::make_unique<detail::DividerComponent>(orientation,
                                                              thickness, color);
          },
          {}};
}
} // namespace ui
