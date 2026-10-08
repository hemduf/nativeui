#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/detail/layout_types.hpp>

namespace ui {

/// Retained vertical layout with main-axis gap, uniform inset padding,
/// main-axis justification and cross-axis alignment in logical pixels.
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

/// Detached vertical composition builder owning child Specs.
/// Default gap is 16 and default padding is 24 logical UI units.
class Column {
public:
    template <class... Children>
    explicit Column(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

/// Set vertical gap in logical pixels.
    Column&& gap(float value) &&;

/// Set uniform layout padding in logical pixels.
    Column&& padding(float value) &&;

/// Set horizontal cross-axis child alignment.
    Column&& align(Align value) &&;

/// Set vertical main-axis distribution.
    Column&& justify(Justify value) &&;

/// Consume the owned child Specs and layout policy.
    Spec spec() &&;

private:
    float gap_{16.0f};
    float padding_{24.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};

} // namespace ui
