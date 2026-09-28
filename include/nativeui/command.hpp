#pragma once

#include <nativeui/component.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Callback used by `CommandScope`. Return `Handled` to stop command
/// bubbling or `Ignored` to allow an ancestor/global handler to try.
using CommandCallback = std::function<EventResult(Command)>;

/// Transparent retained-mode wrapper that handles portable semantic commands.
///
/// Only `InputType::Command` with a non-None command invokes the callback.
/// Ordinary pointer/key/text events continue through normal descendant
/// targeting. Returning `Ignored` lets command routing continue through
/// retained ancestors and eventually the tree's global command handler.
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

/// Declarative one-child command handler.
///
/// The scope does not create focus or intercept raw key events. NativeUI first
/// converts supported primary-modifier shortcuts into semantic commands, routes
/// from the focused leaf through ancestors, and invokes this callback only if
/// the route reaches the scope.
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
