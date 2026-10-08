#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// One-child wrapper advertising dimensionless grow/shrink weights to its
/// retained parent while forwarding measurement/layout to the child.
class FlexComponent final : public Component {
public:
    FlexComponent(float grow, float shrink);

    [[nodiscard]] FlexFactors flex_factors() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    FlexFactors factors_{};
};

/// Detached one-child flex builder. Weights influence parent free-space
/// distribution; they do not cause an independent second layout system.
class Flex {
public:
    template <class Child>
    explicit Flex(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

/// Set dimensionless positive free-space distribution weight.
    Flex&& grow(float value) &&;

/// Set dimensionless shrink weight under constrained layout.
    Flex&& shrink(float value) &&;

/// Consume the child and flex factors into a retained Spec.
    Spec spec() &&;

private:
    float grow_{};
    float shrink_{};
    std::vector<Spec> children_;
};

} // namespace ui
