#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

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

class Flex {
public:
    template <class Child>
    explicit Flex(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Flex&& grow(float value) &&;

    Flex&& shrink(float value) &&;

    Spec spec() &&;

private:
    float grow_{};
    float shrink_{};
    std::vector<Spec> children_;
};

} // namespace ui
