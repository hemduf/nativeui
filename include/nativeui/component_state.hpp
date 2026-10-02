#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

class AvailabilityWrapperComponent : public Component, public ThemeBinding {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    // Availability decorators paint no pixels of their own. Descendants classify
    // their resolved Enabled/ReadOnly presentation independently, while Tree
    // already handles visibility changes explicitly. Do not let the decorator
    // itself force a full-viewport repaint when its own presentation is unchanged.
    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability&,
        const ComponentAvailability&) const noexcept override {
        return false;
    }

    void paint(PaintContext&) const override {}
};

class VisibilityComponent final : public AvailabilityWrapperComponent {
public:
    explicit VisibilityComponent(State<VisibilityMode>& state) : mode_state_(&state) {}

    VisibilityComponent(State<bool>& visible, VisibilityMode unavailable_mode)
        : visible_state_(&visible), unavailable_mode_(sanitize_unavailable_mode(unavailable_mode)) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        if (mode_state_) {
            result.visibility = mode_state_->get();
        } else if (visible_state_ && !visible_state_->get()) {
            result.visibility = unavailable_mode_;
        }
        return result;
    }

    void mount(MountContext& context) override {
        auto invalidate = context.availability_invalidator();
        if (mode_state_) {
            mode_subscription_ = mode_state_->observe(
                [invalidate](const VisibilityMode&) { invalidate(); });
        } else if (visible_state_) {
            visible_subscription_ = visible_state_->observe(
                [invalidate](const bool&) { invalidate(); });
        }
    }

    void unmount(LifecycleContext&) override {
        mode_subscription_.reset();
        visible_subscription_.reset();
    }

private:
    [[nodiscard]] static VisibilityMode sanitize_unavailable_mode(VisibilityMode mode) noexcept {
        return mode == VisibilityMode::Visible ? VisibilityMode::Hidden : mode;
    }

    State<VisibilityMode>* mode_state_{};
    State<bool>* visible_state_{};
    VisibilityMode unavailable_mode_{VisibilityMode::Hidden};
    State<VisibilityMode>::Subscription mode_subscription_;
    State<bool>::Subscription visible_subscription_;
};

class EnabledComponent final : public AvailabilityWrapperComponent {
public:
    explicit EnabledComponent(State<bool>& state) : state_(&state) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        result.enabled = state_->get();
        return result;
    }

    void mount(MountContext& context) override {
        subscription_ = state_->observe(
            [invalidate = context.availability_invalidator()](const bool&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

private:
    State<bool>* state_{};
    State<bool>::Subscription subscription_;
};

class ReadOnlyComponent final : public AvailabilityWrapperComponent {
public:
    explicit ReadOnlyComponent(State<bool>& state) : state_(&state) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result{};
        result.read_only = state_->get();
        return result;
    }

    void mount(MountContext& context) override {
        subscription_ = state_->observe(
            [invalidate = context.availability_invalidator()](const bool&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

private:
    State<bool>* state_{};
    State<bool>::Subscription subscription_;
};

} // namespace detail

/// Availability decorator that controls retained subtree visibility from State.
///
/// The referenced State is borrowed by raw address: it must outlive every Spec
/// produced by this builder and every retained wrapper materialized from that
/// Spec. Unlike APIs that store Binding<T>, destroying the State first would
/// violate the lifetime contract. `State<VisibilityMode>` selects
/// Visible/Hidden/Collapsed directly. With `State<bool>`, true means Visible
/// and false defaults to Hidden unless `mode(Collapsed)` is selected.
///
/// State observation and availability reconciliation are synchronous UI-thread
/// work and are not audio/DSP real-time operations. Visibility changes do not
/// mount/unmount the child.
class Visibility {
public:
    /// Borrow a VisibilityMode State and own one declarative child Spec.
    ///
    /// `state` is not copied or converted to Binding and must satisfy the
    /// class lifetime contract through later Spec materialization/mounting.
    template <class Child>
    Visibility(State<VisibilityMode>& state, Child&& child) : mode_state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Borrow a bool State where true is Visible and false uses `mode()`.
    ///
    /// The State is borrowed for the lifetime described by the class contract.
    template <class Child>
    Visibility(State<bool>& visible, Child&& child) : visible_state_(&visible) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// For a bool visibility state, choose whether false means Hidden or
    /// Collapsed. Passing Visible is sanitized to Hidden so false always makes
    /// the subtree unavailable.
    ///
    /// The setting is ignored by the State<VisibilityMode> form, whose State
    /// value already selects the complete mode. This rvalue-qualified mutation
    /// returns the same builder and performs no retained-tree work.
    Visibility&& mode(VisibilityMode value) && {
        unavailable_mode_ = value == VisibilityMode::Visible ? VisibilityMode::Hidden : value;
        return std::move(*this);
    }

    /// Consume the builder into a one-child availability Spec.
    ///
    /// The Spec captures the selected State pointer; it does not extend State
    /// lifetime. Later materialization may allocate and must occur while the
    /// borrowed State is still alive.
    Spec spec() && {
        auto* mode_state = mode_state_;
        auto* visible_state = visible_state_;
        const auto unavailable_mode = unavailable_mode_;
        if (mode_state) {
            return Spec{
                [mode_state] { return std::make_unique<detail::VisibilityComponent>(*mode_state); },
                std::move(children_)};
        }
        return Spec{
            [visible_state, unavailable_mode] {
                return std::make_unique<detail::VisibilityComponent>(
                    *visible_state, unavailable_mode);
            },
            std::move(children_)};
    }

private:
    State<VisibilityMode>* mode_state_{};
    State<bool>* visible_state_{};
    VisibilityMode unavailable_mode_{VisibilityMode::Hidden};
    std::vector<Spec> children_;
};

/// Availability decorator that enables/disables one retained subtree.
///
/// The referenced `State<bool>` is borrowed by raw address and must outlive
/// every Spec produced by this builder plus every retained wrapper materialized
/// from it. Effective enabled state is inherited monotonically through
/// ancestors: a disabled ancestor cannot be re-enabled by a descendant.
/// Disabled content remains laid out/painted but is excluded from normal
/// interactive targeting and focus eligibility.
class Enabled {
public:
    /// Borrow an enabled State and own one declarative child Spec.
    ///
    /// Construction does not subscribe or mutate the Tree. The State is not
    /// retained through Binding and must obey the class lifetime contract.
    template <class Child>
    Enabled(State<bool>& state, Child&& child) : state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Consume the builder into a Spec that retains the borrowed State pointer.
    ///
    /// Materialization/observation happens later on the UI thread and may
    /// allocate/throw through ordinary retained component construction.
    Spec spec() && {
        auto* state = state_;
        return Spec{
            [state] { return std::make_unique<detail::EnabledComponent>(*state); },
            std::move(children_)};
    }

private:
    State<bool>* state_{};
    std::vector<Spec> children_;
};

/// Availability decorator that marks one retained subtree read-only.
///
/// The referenced `State<bool>` is borrowed by raw address and must outlive
/// every Spec produced by this builder plus every retained wrapper materialized
/// from it. Effective read-only state is inherited monotonically. Read-only
/// does not itself remove focus or hit-test eligibility; editable/value
/// components decide which mutating actions to reject while preserving
/// non-mutating interactions such as navigation/selection where applicable.
class ReadOnly {
public:
    /// Borrow a read-only State and own one declarative child Spec.
    ///
    /// Construction does not subscribe or mutate the Tree. The State remains
    /// application-owned and must obey the class lifetime contract.
    template <class Child>
    ReadOnly(State<bool>& state, Child&& child) : state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Consume the builder into a Spec that retains the borrowed State pointer.
    ///
    /// Later State notifications synchronously request availability
    /// reconciliation in the owning UI/main-thread domain.
    Spec spec() && {
        auto* state = state_;
        return Spec{
            [state] { return std::make_unique<detail::ReadOnlyComponent>(*state); },
            std::move(children_)};
    }

private:
    State<bool>* state_{};
    std::vector<Spec> children_;
};

} // namespace ui
