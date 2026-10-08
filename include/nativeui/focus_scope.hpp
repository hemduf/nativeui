#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/state.hpp>
#include <cstddef>
#include <functional>

namespace ui {

/// Retained focus-only boundary. Inactive excludes focus targeting but does
/// not hide, collapse or unmount child content. Binding changes invalidate
/// focus structure and presentation during the mounted UI lifetime.
class FocusScopeComponent final : public Component {
public:
/// Own the active Binding and focus policy without moving focus at construction.
    FocusScopeComponent(Binding<bool> active, bool trap, std::size_t default_index);

    [[nodiscard]] bool is_focus_scope() const noexcept override;
/// Read the Binding's last committed active flag.
    [[nodiscard]] bool focus_scope_active() const noexcept override;
/// Report the active-scope Tab/Shift+Tab trapping policy.
    [[nodiscard]] bool focus_scope_traps() const noexcept override;
/// Return the zero-based index among currently available focusable descendants.
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override;

/// Subscribe to active-state changes and invalidate focus/presentation.
    void mount(MountContext& context) override;

/// Unregister the mounted subscription before teardown.
    void unmount(LifecycleContext&) override;
    void paint(PaintContext&) const override;

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    Binding<bool>::Subscription subscription_;
};

/// Declarative one-child focus boundary; visibility and layout are separate
/// from focus eligibility. Active scopes may trap traversal and request a
/// default focusable descendant. Work belongs to the owning UI thread.
class FocusScope {
public:
    template <class Child>
/// Own the Binding handle and convert child to Spec.
    FocusScope(Binding<bool> active, Child&& child) : active_(std::move(active)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
/// Convenience overload retaining State::binding(), not the State object.
    FocusScope(State<bool>& active, Child&& child)
        : FocusScope(active.binding(), std::forward<Child>(child)) {}

/// Enable or disable trapping Tab traversal inside this active scope.
    FocusScope&& trap(bool value = true) &&;

/// Select a zero-based available focusable descendant, not direct child.
/// An out-of-range index falls back to the first eligible descendant.
    FocusScope&& default_focus(std::size_t focusable_descendant_index) &&;

/// Consume the child, Binding and policy into a retained Spec.
    Spec spec() &&;

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    std::vector<Spec> children_;
};

} // namespace ui
