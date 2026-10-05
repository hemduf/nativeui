#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

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

class Clip {
public:
    template <class Child>
    explicit Clip(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() &&;

private:
    std::vector<Spec> children_;
};

} // namespace ui
