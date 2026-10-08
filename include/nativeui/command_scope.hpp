#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/state.hpp>
#include <cstddef>
#include <functional>

namespace ui {

/// Owned semantic-command callback on the UI thread. Handled stops routing;
/// Ignored permits ancestor/global fallback. Empty callbacks are ignored.
/// Exceptions propagate through retained dispatch, not into EventResult.
using CommandCallback = std::function<EventResult(Command)>;

/// Transparent one-child retained boundary for semantic commands.
/// Only a non-None InputType::Command invokes the callback; it neither
/// takes focus nor paints or intercepts normal pointer/raw keyboard input.
class CommandScopeComponent final : public Component {
public:
/// Take ownership of a callback, including an optional empty function.
    explicit CommandScopeComponent(CommandCallback callback);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override;

/// Handle non-None semantic commands or return Ignored for other input.
    EventResult input(const InputEvent& event, InputContext&) override;

    void paint(PaintContext&) const override;

private:
    CommandCallback callback_;
};

/// Declarative command callback along the focused ancestor route.
/// Descendant handlers run first; an ignored result can reach ancestor or
/// global command handlers. This scope does not claim keyboard focus.
class CommandScope {
public:
    template <class Child>
/// Own both callback and child Spec; constructing does not invoke callback.
    CommandScope(CommandCallback callback, Child&& child)
        : callback_(std::move(callback)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

/// Consume callback and child into a retained Spec.
    Spec spec() &&;

private:
    CommandCallback callback_;
    std::vector<Spec> children_;
};

} // namespace ui
