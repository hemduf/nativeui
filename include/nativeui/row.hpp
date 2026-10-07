#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/detail/layout_types.hpp>

namespace ui {

class RowComponent final : public Component {
public:
    RowComponent(float gap, Align align = Align::Start, Justify justify = Justify::Start);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    float gap_{};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
};

class Row {
public:
    template <class... Children>
    explicit Row(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Row&& gap(float value) &&;

    Row&& align(Align value) &&;

    Row&& justify(Justify value) &&;

    Spec spec() &&;

private:
    float gap_{18.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};

} // namespace ui
