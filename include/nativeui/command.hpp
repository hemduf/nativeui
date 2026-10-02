#pragma once

#include <nativeui/component.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Owned callback used by `CommandScope` for one semantic command dispatch.
///
/// The Command value is passed by value and has no borrowed lifetime. Returning
/// `Handled` stops command bubbling; `Ignored` keeps ancestor/global fallback
/// eligible. Invocation is synchronous on the owning UI thread. Captures follow
/// ordinary std::function ownership rules, and exceptions are not translated
/// into EventResult by CommandScope; they propagate through the Tree/UI dispatch
/// exception boundary. This callback is not an audio/DSP real-time API.
using CommandCallback = std::function<EventResult(Command)>;

/// Transparent retained-mode wrapper that handles portable semantic commands.
///
/// Only `InputType::Command` with a non-None command invokes the callback.
/// Ordinary pointer/key/text events continue through normal descendant
/// targeting. Returning `Ignored` lets command routing continue through
/// retained ancestors and eventually the tree's global command handler.
class CommandScopeComponent final : public Component {
public:
    /// Take ownership of the command callback for this retained component.
    ///
    /// An empty std::function is valid and behaves as an always-Ignored handler.
    explicit CommandScopeComponent(CommandCallback callback)
        : callback_(std::move(callback)) {}

    /// Forward preferred logical measurement from the sole child, or zero.
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    /// Forward minimum logical measurement from the sole child, or zero.
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    /// Give the sole child the complete logical bounds assigned to this wrapper.
    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    /// Handle only non-None semantic Command events and otherwise return Ignored.
    ///
    /// `event` and `InputContext` are borrowed for the synchronous dispatch
    /// call; this implementation does not retain either. The owned callback may
    /// synchronously mutate application State or trigger retained work. Reentrant
    /// structural effects follow the Tree/UI safe-checkpoint dispatch contract.
    /// A callback exception propagates rather than being converted to Ignored.
    EventResult input(const InputEvent& event, InputContext&) override {
        if (event.type != InputType::Command || event.command == Command::None || !callback_) {
            return EventResult::Ignored;
        }
        return callback_(event.command);
    }

    /// Paint no pixels; the scope affects semantic command routing only.
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
    /// Build a one-child scope that owns `callback` and the child's Spec.
    ///
    /// Child conversion may allocate/throw. Construction performs no retained
    /// mounting, input dispatch or callback invocation.
    template <class Child>
    CommandScope(CommandCallback callback, Child&& child)
        : callback_(std::move(callback)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Consume the builder into a declarative Spec.
    ///
    /// The callback is transferred into the retained component when the Spec is
    /// materialized; its captures live with that component. Materialization and
    /// later invocation occur in the owning UI/main-thread domain.
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
