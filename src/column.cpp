#include <nativeui/column.hpp>
#include <algorithm>
#include <cmath>
#include "detail/layout_support.hpp"

namespace ui {

ColumnComponent::ColumnComponent(
    float gap,
    float padding,
    Align align ,
    Justify justify )
    : gap_(std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f),
      padding_(std::isfinite(padding) ? std::max(0.0f, padding) : 0.0f),
      align_(align),
      justify_(justify) {}

Size ColumnComponent::measure(const std::vector<ChildMetrics>& children) const {
    const double inset = static_cast<double>(padding_) * 2.0;
    double width = inset;
    double height = inset;
    bool has_previous = false;
    for (const auto& child : children) {
        if (!child.participates_in_layout) continue;
        width = std::max(width, child.preferred.w + inset);
        if (has_previous) height += gap_;
        height += child.preferred.h;
        has_previous = true;
    }
    return {detail::saturating_extent(width), detail::saturating_extent(height)};
}

Size ColumnComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    const double inset = static_cast<double>(padding_) * 2.0;
    double width = inset;
    double height = inset;
    bool has_previous = false;
    for (const auto& child : children) {
        if (!child.participates_in_layout) continue;
        width = std::max(width, child.minimum.w + inset);
        if (has_previous) height += gap_;
        height += child.minimum.h;
        has_previous = true;
    }
    return {detail::saturating_extent(width), detail::saturating_extent(height)};
}

Constraints ColumnComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    const auto inner = constraints.inset(padding_, padding_);
    // Column constrains child width but allows natural vertical measurement.
    return Constraints::loose({inner.max.w, kUnboundedExtent});
}

void ColumnComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    const double inner_x = static_cast<double>(bounds.x) + padding_;
    const double inner_y = static_cast<double>(bounds.y) + padding_;
    const double inset = static_cast<double>(padding_) * 2.0;
    const float available_w = detail::saturating_extent(bounds.w - inset);
    const float available_h = detail::saturating_extent(bounds.h - inset);

    const auto allocation = detail::allocate_main_axis(
        children, available_h, gap_, false);
    const auto participant_count = static_cast<std::size_t>(std::count_if(
        children.begin(), children.end(),
        [](const ChildMetrics& child) { return child.participates_in_layout; }));
    const float actual_gap = detail::distributed_gap(
        justify_, gap_, allocation.free_space, participant_count);
    double y = inner_y + detail::main_axis_offset(justify_, allocation.free_space);

    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i].participates_in_layout) {
            placements[i].bounds = Rect{bounds.x, bounds.y, 0.0f, 0.0f};
            continue;
        }
        const auto size = children[i].preferred;
        const float child_h = allocation.extents[i];
        const float natural_w = std::min(std::max(0.0f, size.w), available_w);
        const float child_w = align_ == Align::Stretch ? available_w : natural_w;
        const float child_x = detail::saturating_coordinate(
            inner_x + detail::cross_axis_offset(align_, available_w, child_w));
        placements[i].bounds = Rect{
            child_x, detail::saturating_coordinate(y), child_w, child_h};
        y += child_h + actual_gap;
    }
}

void ColumnComponent::paint(PaintContext&) const {}

Column&& Column::gap(float value) && {
    gap_ = value;
    return std::move(*this);
}

Column&& Column::padding(float value) && {
    padding_ = value;
    return std::move(*this);
}

Column&& Column::align(Align value) && {
    align_ = value;
    return std::move(*this);
}

Column&& Column::justify(Justify value) && {
    justify_ = value;
    return std::move(*this);
}

Spec Column::spec() && {
    const float gap = gap_;
    const float padding = padding_;
    const auto align = align_;
    const auto justify = justify_;
    return Spec{
        [gap, padding, align, justify] {
            return std::make_unique<ColumnComponent>(gap, padding, align, justify);
        },
        std::move(children_)};
}

} // namespace ui
