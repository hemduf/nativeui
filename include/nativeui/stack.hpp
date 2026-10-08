#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Retained overlapping-child layout with shared logical bounds.
/// Order of child recipes determines painting and pointer hit-test precedence.
class StackComponent final : public Component {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;
};

/// Detached builder owning overlapping child Specs.
class Stack {
public:
    template <class... Children>
/// Convert all children to Specs, possibly allocating or invoking user code.
    explicit Stack(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

/// Consume the owned child recipes into one retained Stack.
    Spec spec() &&;

private:
    std::vector<Spec> children_;
};

} // namespace ui
