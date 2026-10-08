#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Uniform logical inset layout around one retained child.
/// Parent measurement includes the inset; child placement receives the
/// remaining content bounds.
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

/// Detached one-child padding builder, in logical UI units.
class Padding {
public:
    template <class Child>
/// Own the child Spec and uniform padding value; no retained node mounts yet.
    Padding(float padding, Child&& child) : padding_(padding) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

/// Consume the child and padding value into retained layout.
    Spec spec() &&;

private:
    float padding_{};
    std::vector<Spec> children_;
};

} // namespace ui
