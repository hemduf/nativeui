#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Retained implementation of a focus-only subtree boundary.
///
/// Rendering, layout and ordinary visibility are independent: `active=false`
/// removes descendants from focus targeting but does not hide or unmount them.
/// The active binding is observed while mounted and focus structure is
/// invalidated when it changes.
class FocusScopeComponent final : public Component {
public:
    FocusScopeComponent(Binding<bool> active, bool trap, std::size_t default_index)
        : active_(std::move(active)), trap_(trap), default_index_(default_index) {}

    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return active_.get(); }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return trap_; }
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override {
        return default_index_;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext& context) override {
        subscription_ = active_.observe(
            [invalidate_focus = context.focus_invalidator(),
             invalidate = context.invalidator()](const bool&) {
                invalidate_focus();
                invalidate();
            });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }
    void paint(PaintContext&) const override {}

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    Binding<bool>::Subscription subscription_;
};

/// Declarative focus boundary for one child subtree.
///
/// An active scope may optionally trap traversal inside itself. On activation,
/// `default_focus()` selects a zero-based focusable-descendant index; if the
/// requested index is unavailable/out of range, the first available descendant
/// is used. Deactivation restores the previously focused eligible target when
/// possible, otherwise normal tree fallback rules apply.
///
/// Focus/state mutation is UI-thread work. The supplied Binding/State must obey
/// the lifetime rules documented by NativeUI's state/binding contract.
class FocusScope {
public:
    template <class Child>
    FocusScope(Binding<bool> active, Child&& child) : active_(std::move(active)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    FocusScope(State<bool>& active, Child&& child)
        : FocusScope(active.binding(), std::forward<Child>(child)) {}

    /// When true (the default), Tab/Shift+Tab traversal cannot leave this scope
    /// while focus is inside an active scope.
    FocusScope&& trap(bool value = true) && {
        trap_ = value;
        return std::move(*this);
    }

    /// Select the zero-based available focusable descendant preferred when the
    /// scope becomes active.
    FocusScope&& default_focus(std::size_t focusable_descendant_index) && {
        default_index_ = focusable_descendant_index;
        return std::move(*this);
    }

    Spec spec() && {
        auto active = active_;
        const bool trap = trap_;
        const auto default_index = default_index_;
        return Spec{
            [active, trap, default_index] {
                return std::make_unique<FocusScopeComponent>(active, trap, default_index);
            },
            std::move(children_)};
    }

private:
    Binding<bool> active_;
    bool trap_{true};
    std::size_t default_index_{};
    std::vector<Spec> children_;
};

} // namespace ui
