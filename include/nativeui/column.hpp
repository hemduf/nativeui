#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/detail/layout_types.hpp>

namespace ui {

class ColumnComponent final : public Component {
public:
    ColumnComponent(
        float gap,
        float padding,
        Align align = Align::Start,
        Justify justify = Justify::Start);

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
    float padding_{};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
};

class Column {
public:
    template <class... Children>
    explicit Column(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Column&& gap(float value) &&;

    Column&& padding(float value) &&;

    Column&& align(Align value) &&;

    Column&& justify(Justify value) &&;

    Spec spec() &&;

private:
    float gap_{16.0f};
    float padding_{24.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};

} // namespace ui
