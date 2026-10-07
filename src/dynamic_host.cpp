#include <nativeui/detail/dynamic_host.hpp>

#include <algorithm>

namespace ui::detail {
Size DynamicHostComponent::measure(const std::vector<ChildMetrics>& children) const {
    Size result;
    for (const auto& child : children) {
        result.w = std::max(result.w,child.preferred.w);
        result.h = std::max(result.h,child.preferred.h);
    }
    return result;
}
void DynamicHostComponent::layout_children(Rect bounds,const std::vector<ChildMetrics>& children,
                                          std::vector<ChildPlacement>& placements) const {
    const auto count = std::min(children.size(),placements.size());
    for (std::size_t i = 0; i < count; ++i) placements[i].bounds = bounds;
}
void DynamicHostComponent::paint(PaintContext&) const {}
} // namespace ui::detail
