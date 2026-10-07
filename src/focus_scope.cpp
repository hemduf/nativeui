#include <nativeui/focus_scope.hpp>

namespace ui {

FocusScopeComponent::FocusScopeComponent(Binding<bool> active, bool trap, std::size_t default_index)
    : active_(std::move(active)), trap_(trap), default_index_(default_index) {}

bool FocusScopeComponent::is_focus_scope() const noexcept { return true; }

bool FocusScopeComponent::focus_scope_active() const noexcept { return active_.get(); }

bool FocusScopeComponent::focus_scope_traps() const noexcept { return trap_; }

std::size_t FocusScopeComponent::focus_scope_default_index() const noexcept {
    return default_index_;
}

Size FocusScopeComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

ChildMetrics FocusScopeComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    return result;
}

Size FocusScopeComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

void FocusScopeComponent::layout_children(Rect bounds,
                     const std::vector<ChildMetrics>&,
                     std::vector<ChildPlacement>& placements) const {
    if (!placements.empty()) placements.front().bounds = bounds;
}

void FocusScopeComponent::mount(MountContext& context) {
    subscription_ = active_.observe(
        [invalidate_focus = context.focus_invalidator(),
         invalidate = context.invalidator()](const bool&) {
            invalidate_focus();
            invalidate();
        });
}

void FocusScopeComponent::unmount(LifecycleContext&) { subscription_.reset(); }

void FocusScopeComponent::paint(PaintContext&) const {}

FocusScope&& FocusScope::trap(bool value ) && {
    trap_ = value;
    return std::move(*this);
}

FocusScope&& FocusScope::default_focus(std::size_t focusable_descendant_index) && {
    default_index_ = focusable_descendant_index;
    return std::move(*this);
}

Spec FocusScope::spec() && {
    auto active = active_;
    const bool trap = trap_;
    const auto default_index = default_index_;
    return Spec{
        [active, trap, default_index] {
            return std::make_unique<FocusScopeComponent>(active, trap, default_index);
        },
        std::move(children_)};
}

} // namespace ui
