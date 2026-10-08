#include <nativeui/command_scope.hpp>

namespace ui {

CommandScopeComponent::CommandScopeComponent(CommandCallback callback)
    : callback_(std::move(callback)) {}

Size CommandScopeComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

ChildMetrics CommandScopeComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    return result;
}

Size CommandScopeComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

void CommandScopeComponent::layout_children(Rect bounds,
                     const std::vector<ChildMetrics>&,
                     std::vector<ChildPlacement>& placements) const {
    if (!placements.empty()) placements.front().bounds = bounds;
}

EventResult CommandScopeComponent::input(const InputEvent& event, InputContext&) {
    if (event.type != InputType::Command || event.command == Command::None || !callback_) {
        return EventResult::Ignored;
    }
    return callback_(event.command);
}

void CommandScopeComponent::paint(PaintContext&) const {}

Spec CommandScope::spec() && {
    auto callback = std::move(callback_);
    return Spec{
        [callback = std::move(callback)]() mutable {
            return std::make_unique<CommandScopeComponent>(std::move(callback));
        },
        std::move(children_)};
}

} // namespace ui
