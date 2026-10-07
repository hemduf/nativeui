#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

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

class Stack {
public:
    template <class... Children>
    explicit Stack(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Spec spec() &&;

private:
    std::vector<Spec> children_;
};

} // namespace ui
