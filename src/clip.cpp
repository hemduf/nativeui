#include <nativeui/clip.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

bool ClipComponent::clips_children() const noexcept { return true; }

Size ClipComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

Size ClipComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

Constraints ClipComponent::child_constraints(
    const Constraints&, std::size_t, std::size_t) const {
    // A clip is a viewport, not a sizing container: measure its content at
    // natural size so the descendant can intentionally overflow the clip.
    return Constraints::unbounded();
}

void ClipComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>& children,
    std::vector<ChildPlacement>& placements) const {
    if (placements.empty() || children.empty()) return;
    const auto size = children.front().preferred;
    placements.front().bounds = Rect{
        bounds.x, bounds.y, std::max(0.0f, size.w), std::max(0.0f, size.h)};
}

void ClipComponent::paint(PaintContext&) const {}

Spec Clip::spec() && {
    return Spec{[] { return std::make_unique<ClipComponent>(); }, std::move(children_)};
}

} // namespace ui
