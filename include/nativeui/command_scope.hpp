#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/state.hpp>
#include <cstddef>
#include <functional>

namespace ui {

using CommandCallback = std::function<EventResult(Command)>;

class CommandScopeComponent final : public Component {
public:
    explicit CommandScopeComponent(CommandCallback callback);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override;

    EventResult input(const InputEvent& event, InputContext&) override;

    void paint(PaintContext&) const override;

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

    Spec spec() &&;

private:
    CommandCallback callback_;
    std::vector<Spec> children_;
};

} // namespace ui
