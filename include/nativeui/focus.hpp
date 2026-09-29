#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Retained implementation of a one-child focus-only subtree boundary.
///
/// Rendering, layout and ordinary visibility are independent: `active=false`
/// removes descendants from focus targeting but does not hide, collapse or
/// unmount them. The component owns a copyable `Binding<bool>` handle, so the
/// originating `State<bool>` may be destroyed without leaving a dangling
/// reference; a dead source leaves the binding readable at its last retained
/// value but no longer produces changes.
///
/// While mounted, changes invalidate focus structure and retained presentation.
/// All methods participate in the owning Tree/UI main-thread lifecycle and are
/// not audio/DSP real-time operations.
class FocusScopeComponent final : public Component {
public:
    /// Store the active binding and focus policy for this retained scope.
    ///
    /// `active` is owned by value. `default_index` is a zero-based index in
    /// the currently available focusable-descendant order, not a child index.
    /// Construction performs no focus mutation; policy takes effect once the
    /// component is mounted/reconciled by its owning Tree.
    FocusScopeComponent(Binding<bool> active, bool trap, std::size_t default_index)
        : active_(std::move(active)), trap_(trap), default_index_(default_index) {}

    /// Identify this component as a retained focus-scope boundary.
    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }

    /// Return the current active value from the owned Binding handle.
    ///
    /// This is a UI-domain state read. If the source State has died, Binding
    /// semantics return the retained last value rather than dereferencing it.
    [[nodiscard]] bool focus_scope_active() const noexcept override { return active_.get(); }

    /// Return whether keyboard focus traversal is trapped inside an active scope.
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return trap_; }

    /// Return the preferred zero-based available-focusable descendant index.
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override {
        return default_index_;
    }

    /// Forward preferred measurement from the sole child, or zero for no child.
    ///
    /// Child metrics and the returned Size use NativeUI logical layout units.
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    /// Forward minimum measurement from the sole child, or zero for no child.
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    /// Give the sole child the complete logical bounds assigned to this wrapper.
    ///
    /// Extra placement entries, if supplied by a custom caller, are untouched.
    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    /// Subscribe to active-state changes for the mounted lifetime.
    ///
    /// The context is borrowed only for this call. Stored invalidator functors
    /// are lifetime-safe retained handles supplied by the Tree. Registration may
    /// allocate/throw; normal lifecycle rollback is owned by the Tree/UI.
    void mount(MountContext& context) override {
        subscription_ = active_.observe(
            [invalidate_focus = context.focus_invalidator(),
             invalidate = context.invalidator()](const bool&) {
                invalidate_focus();
                invalidate();
            });
    }

    /// Stop observing the active binding before retained teardown completes.
    void unmount(LifecycleContext&) override { subscription_.reset(); }

    /// Paint no pixels; the scope affects focus routing only.
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
/// Focus/state mutation is UI-thread work. The Binding overload owns its source
/// handle by value. The State overload immediately converts the State to a
/// Binding, so the resulting builder/Spec does not retain a raw State reference;
/// after State destruction it follows Binding's documented retained-last-value
/// semantics. Child conversion/spec allocation may throw in the ordinary C++ way.
class FocusScope {
public:
    /// Build a one-child focus scope driven by an owned Binding handle.
    ///
    /// `child` is converted to an owned declarative Spec during construction.
    /// No retained Tree node is mounted and no focus callback runs here.
    template <class Child>
    FocusScope(Binding<bool> active, Child&& child) : active_(std::move(active)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Build a scope from State without borrowing the State object itself.
    ///
    /// `active.binding()` is captured immediately; the State object therefore
    /// need not outlive a later mounted FocusScope component.
    template <class Child>
    FocusScope(State<bool>& active, Child&& child)
        : FocusScope(active.binding(), std::forward<Child>(child)) {}

    /// When true (the default), Tab/Shift+Tab traversal cannot leave this scope
    /// while focus is inside an active scope.
    ///
    /// This is an rvalue-qualified builder mutation and returns the same builder.
    FocusScope&& trap(bool value = true) && {
        trap_ = value;
        return std::move(*this);
    }

    /// Select the zero-based available focusable descendant preferred when the
    /// scope becomes active.
    ///
    /// The index is resolved against currently eligible focusable descendants,
    /// not direct children. Missing/out-of-range targets fall back to the first
    /// available descendant under the Tree focus rules.
    FocusScope&& default_focus(std::size_t focusable_descendant_index) && {
        default_index_ = focusable_descendant_index;
        return std::move(*this);
    }

    /// Consume the builder into an owned declarative Spec.
    ///
    /// The returned Spec owns the child specification, active Binding and focus
    /// policy needed to materialize the retained component later. It borrows no
    /// State object. Materialization/lifecycle work remains UI-thread work.
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
