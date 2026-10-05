#include <nativeui/row.hpp>
#include <algorithm>
#include <cmath>
#include "detail/layout_support.hpp"

namespace ui {

RowComponent::RowComponent(float gap, Align align , Justify justify )
    : gap_(std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f),
      align_(align),
      justify_(justify) {}

Size RowComponent::measure(const std::vector<ChildMetrics>& children) const {
    Size result{};
    bool has_previous = false;
    for (const auto& child : children) {
        if (!child.participates_in_layout) continue;
        if (has_previous) result.w += gap_;
        result.w += child.preferred.w;
        result.h = std::max(result.h, child.preferred.h);
        has_previous = true;
    }
    return result;
}

Size RowComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    Size result{};
    bool has_previous = false;
    for (const auto& child : children) {
        if (!child.participates_in_layout) continue;
        if (has_previous) result.w += gap_;
        result.w += child.minimum.w;
        result.h = std::max(result.h, child.minimum.h);
        has_previous = true;
    }
    return result;
}

Constraints RowComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    // Row measures intrinsic widths without prematurely constraining the main
    // axis; flex allocation happens during placement. Height stays bounded.
    return Constraints::loose({kUnboundedExtent, constraints.max.h});
}

void RowComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    const float available_w = std::max(0.0f, bounds.w);
    const float available_h = std::max(0.0f, bounds.h);

    const auto allocation = detail::allocate_main_axis(
        children, available_w, gap_, true);
    const auto participant_count = static_cast<std::size_t>(std::count_if(
        children.begin(), children.end(),
        [](const ChildMetrics& child) { return child.participates_in_layout; }));
    const float actual_gap = detail::distributed_gap(
        justify_, gap_, allocation.free_space, participant_count);
    float x = bounds.x + detail::main_axis_offset(justify_, allocation.free_space);

    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i].participates_in_layout) {
            placements[i].bounds = Rect{bounds.x, bounds.y, 0.0f, 0.0f};
            continue;
        }
        const auto size = children[i].preferred;
        const float child_w = allocation.extents[i];
        const float natural_h = std::min(std::max(0.0f, size.h), available_h);
        const float child_h = align_ == Align::Stretch ? available_h : natural_h;
        const float child_y =
            bounds.y + detail::cross_axis_offset(align_, available_h, child_h);
        placements[i].bounds = Rect{x, child_y, child_w, child_h};
        x += child_w + actual_gap;
    }
}

void RowComponent::paint(PaintContext&) const {}

Row&& Row::gap(float value) && {
    gap_ = value;
    return std::move(*this);
}

Row&& Row::align(Align value) && {
    align_ = value;
    return std::move(*this);
}

Row&& Row::justify(Justify value) && {
    justify_ = value;
    return std::move(*this);
}

Spec Row::spec() && {
    const float gap = gap_;
    const auto align = align_;
    const auto justify = justify_;
    return Spec{
        [gap, align, justify] {
            return std::make_unique<RowComponent>(gap, align, justify);
        },
        std::move(children_)};
}

} // namespace ui
