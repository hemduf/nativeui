#include <nativeui/padding.hpp>
#include "detail/layout_support.hpp"
#include <algorithm>
#include <cmath>

namespace ui {

PaddingComponent::PaddingComponent(float padding) : padding_(std::isfinite(padding) ? std::max(0.0f, padding) : 0.0f) {}

Size PaddingComponent::measure(const std::vector<ChildMetrics>& children) const {
    const auto inset = static_cast<double>(padding_) * 2.0;
    const auto child = children.empty() ? Size{} : children.front().preferred;
    return {detail::saturating_extent(child.w + inset),
            detail::saturating_extent(child.h + inset)};
}

ChildMetrics PaddingComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    if (result.first_baseline) result.first_baseline = detail::saturating_extent(
        static_cast<double>(*result.first_baseline)+padding_);
    return result;
}

Size PaddingComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    const auto inset = static_cast<double>(padding_) * 2.0;
    const auto child = children.empty() ? Size{} : children.front().minimum;
    return {detail::saturating_extent(child.w + inset),
            detail::saturating_extent(child.h + inset)};
}

Constraints PaddingComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    return constraints.inset(padding_, padding_).loosen();
}

void PaddingComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const {
    if (placements.empty()) return;
    const auto inset = static_cast<double>(padding_) * 2.0;
    placements.front().bounds = Rect{
        detail::saturating_coordinate(static_cast<double>(bounds.x) + padding_),
        detail::saturating_coordinate(static_cast<double>(bounds.y) + padding_),
        detail::saturating_extent(bounds.w - inset),
        detail::saturating_extent(bounds.h - inset)};
}

void PaddingComponent::paint(PaintContext&) const {}

Spec Padding::spec() && {
    const auto padding = padding_;
    return Spec{[padding] { return std::make_unique<PaddingComponent>(padding); },
                std::move(children_)};
}

} // namespace ui
