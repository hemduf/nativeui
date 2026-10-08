#include <nativeui/detail/availability_wrapper.hpp>

namespace ui {

Size detail::AvailabilityWrapperComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

ChildMetrics detail::AvailabilityWrapperComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    return result;
}

Size detail::AvailabilityWrapperComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

Constraints detail::AvailabilityWrapperComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    return constraints;
}

void detail::AvailabilityWrapperComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const {
    if (!placements.empty()) placements.front().bounds = bounds;
}

bool detail::AvailabilityWrapperComponent::availability_change_affects_paint(
    const ComponentAvailability&,
    const ComponentAvailability&) const noexcept {
    return false;
}

void detail::AvailabilityWrapperComponent::paint(PaintContext&) const {}

} // namespace ui
