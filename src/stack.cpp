#include <nativeui/stack.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

Size StackComponent::measure(const std::vector<ChildMetrics>& children) const {
    Size result{};
    for (const auto& child : children) {
        result.w = std::max(result.w, child.preferred.w);
        result.h = std::max(result.h, child.preferred.h);
    }
    return result;
}

Size StackComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    Size result{};
    for (const auto& child : children) {
        result.w = std::max(result.w, child.minimum.w);
        result.h = std::max(result.h, child.minimum.h);
    }
    return result;
}

void StackComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const {
    for (auto& placement : placements) placement.bounds = bounds;
}

void StackComponent::paint(PaintContext&) const {}

Spec Stack::spec() && {
    return Spec{[] { return std::make_unique<StackComponent>(); }, std::move(children_)};
}

} // namespace ui
