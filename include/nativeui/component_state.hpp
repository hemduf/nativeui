#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

class AvailabilityWrapperComponent : public Component {
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

class Visibility {
public:
    template <class Child>
    Visibility(State<VisibilityMode>& state, Child&& child) : mode_state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    Visibility(State<bool>& visible, Child&& child) : visible_state_(&visible) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// For a bool visibility state, choose whether false means Hidden or
    /// Collapsed. Passing Visible is sanitized to Hidden so false always makes
    /// the subtree unavailable.
    Visibility&& mode(VisibilityMode value) && {
        unavailable_mode_ = value == VisibilityMode::Visible ? VisibilityMode::Hidden : value;
        return std::move(*this);
    }

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

class Enabled {
public:
    template <class Child>
    Enabled(State<bool>& state, Child&& child) : state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

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

class ReadOnly {
public:
    template <class Child>
    ReadOnly(State<bool>& state, Child&& child) : state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

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
