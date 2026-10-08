#include <nativeui/flex.hpp>
#include <algorithm>
#include <cmath>
#include "detail/layout_support.hpp"

namespace ui {

FlexComponent::FlexComponent(float grow, float shrink)
    : factors_{detail::flex_weight(grow), detail::flex_weight(shrink)} {}

FlexFactors FlexComponent::flex_factors() const noexcept { return factors_; }

Size FlexComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

ChildMetrics FlexComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    return result;
}

Size FlexComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

void FlexComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const {
    if (!placements.empty()) placements.front().bounds = bounds;
}

void FlexComponent::paint(PaintContext&) const {}

Flex&& Flex::grow(float value) && {
    grow_ = value;
    return std::move(*this);
}

Flex&& Flex::shrink(float value) && {
    shrink_ = value;
    return std::move(*this);
}

Spec Flex::spec() && {
    const float grow = grow_;
    const float shrink = shrink_;
    return Spec{
        [grow, shrink] { return std::make_unique<FlexComponent>(grow, shrink); },
        std::move(children_)};
}

} // namespace ui
