#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/detail/layout_types.hpp>

namespace ui {

/// Retained horizontal layout: children are measured then placed in logical
/// coordinates using main-axis justification, cross-axis alignment and gap.
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

/// Detached horizontal layout builder that owns child Specs.
/// Default inter-child gap is 18 logical units; configuration is copied into
/// the retained component when spec() is consumed.
class Row {
public:
    template <class... Children>
    explicit Row(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

/// Set the main-axis gap in logical UI units; layout sanitizes invalid values.
    Row&& gap(float value) &&;

/// Set cross-axis child alignment.
    Row&& align(Align value) &&;

/// Set main-axis distribution of available space.
    Row&& justify(Justify value) &&;

/// Consume child recipes and layout policy into one retained Spec.
    Spec spec() &&;

private:
    float gap_{18.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};

} // namespace ui
