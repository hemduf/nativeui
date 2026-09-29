#pragma once

#include <nativeui/component.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

using CommandCallback = std::function<EventResult(Command)>;

/// Transparent retained-mode wrapper that handles portable commands while
/// allowing ordinary input to continue targeting descendants.
class CommandScopeComponent final : public Component {
public:
    explicit CommandScopeComponent(CommandCallback callback)
        : callback_(std::move(callback)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        if (event.type != InputType::Command || event.command == Command::None || !callback_) {
            return EventResult::Ignored;
        }
        return callback_(event.command);
    }

    void paint(PaintContext&) const override {}

private:
    CommandCallback callback_;
};

class CommandScope {
public:
    template <class Child>
    CommandScope(CommandCallback callback, Child&& child)
        : callback_(std::move(callback)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() && {
        auto callback = std::move(callback_);
        return Spec{
            [callback = std::move(callback)]() mutable {
                return std::make_unique<CommandScopeComponent>(std::move(callback));
            },
            std::move(children_)};
    }

private:
    CommandCallback callback_;
    std::vector<Spec> children_;
};

} // namespace ui
