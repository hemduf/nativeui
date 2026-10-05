#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

class PaddingComponent final : public Component {
public:
    explicit PaddingComponent(float padding);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    float padding_{};
};

class Padding {
public:
    template <class Child>
    Padding(float padding, Child&& child) : padding_(padding) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() &&;

private:
    float padding_{};
    std::vector<Spec> children_;
};

} // namespace ui
