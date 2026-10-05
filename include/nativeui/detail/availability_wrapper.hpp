#pragma once
#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>

namespace ui::detail {

class AvailabilityWrapperComponent : public Component, public ThemeBinding {
public:
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

    // Availability decorators paint no pixels of their own. Descendants classify
    // their resolved Enabled/ReadOnly presentation independently, while Tree
    // already handles visibility changes explicitly. Do not let the decorator
    // itself force a full-viewport repaint when its own presentation is unchanged.
    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability&,
        const ComponentAvailability&) const noexcept override;

    void paint(PaintContext&) const override;
};

} // namespace ui::detail
