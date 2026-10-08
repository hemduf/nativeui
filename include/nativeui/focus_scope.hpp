#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/state.hpp>
#include <cstddef>
#include <functional>

namespace ui {

class FocusScopeComponent final : public Component {
public:
    FocusScopeComponent(Binding<bool> active, bool trap, std::size_t default_index);

    [[nodiscard]] bool is_focus_scope() const noexcept override;
    [[nodiscard]] bool focus_scope_active() const noexcept override;
    [[nodiscard]] bool focus_scope_traps() const noexcept override;
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override;

    void mount(MountContext& context) override;

    void unmount(LifecycleContext&) override;
    void paint(PaintContext&) const override;

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    Binding<bool>::Subscription subscription_;
};

class FocusScope {
public:
    template <class Child>
    FocusScope(Binding<bool> active, Child&& child) : active_(std::move(active)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    FocusScope(State<bool>& active, Child&& child)
        : FocusScope(active.binding(), std::forward<Child>(child)) {}

    FocusScope&& trap(bool value = true) &&;

    FocusScope&& default_focus(std::size_t focusable_descendant_index) &&;

    Spec spec() &&;

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    std::vector<Spec> children_;
};

} // namespace ui
