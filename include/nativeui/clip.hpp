#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// One-child retained clipping boundary for paint and pointer hit testing.
/// Children may retain their natural extent while rendering/input are clipped
/// to this component's assigned logical bounds.
class ClipComponent final : public Component {
public:
    [[nodiscard]] bool clips_children() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints&, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;
};

/// Detached clipping recipe around exactly one child; clipping affects
/// paint and hit testing, not the child's source state.
class Clip {
public:
    template <class Child>
/// Own the child Spec. Conversion may allocate or call user code.
    explicit Clip(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

/// Consume the child into a retained clipping component.
    Spec spec() &&;

private:
    std::vector<Spec> children_;
};

} // namespace ui
