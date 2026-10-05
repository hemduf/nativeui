#pragma once

#include <nativeui/detail/availability_wrapper.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <utility>
#include <vector>
#include <optional>

namespace ui {
namespace detail {

class VisibilityComponent final : public AvailabilityWrapperComponent {
public:
    explicit VisibilityComponent(State<VisibilityMode>& state);
    explicit VisibilityComponent(Binding<VisibilityMode> state);
    VisibilityComponent(State<bool>& visible, VisibilityMode unavailable_mode);
    VisibilityComponent(Binding<bool> visible, VisibilityMode unavailable_mode);

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override;
    void mount(MountContext& context) override;
    void unmount(LifecycleContext&) override;

private:
    [[nodiscard]] static VisibilityMode sanitize_unavailable_mode(VisibilityMode mode) noexcept;

    std::optional<Binding<VisibilityMode>> mode_state_;
    std::optional<Binding<bool>> visible_state_;
    VisibilityMode unavailable_mode_{VisibilityMode::Hidden};
    Binding<VisibilityMode>::Subscription mode_subscription_;
    Binding<bool>::Subscription visible_subscription_;
};

} // namespace detail

class Visibility {
public:
    template <class Child>
    Visibility(State<VisibilityMode>& state, Child&& child)
        : Visibility(state.binding(), std::forward<Child>(child)) {}

    template <class Child>
    Visibility(State<bool>& visible, Child&& child)
        : Visibility(visible.binding(), std::forward<Child>(child)) {}

    template <class Child>
    Visibility(Binding<VisibilityMode> state, Child&& child) : mode_state_(std::move(state)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    Visibility(Binding<bool> visible, Child&& child) : visible_state_(std::move(visible)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Bool false means Hidden or Collapsed; Visible is sanitized to Hidden.
    Visibility&& mode(VisibilityMode value) &&;
    Spec spec() &&;

private:
    std::optional<Binding<VisibilityMode>> mode_state_;
    std::optional<Binding<bool>> visible_state_;
    VisibilityMode unavailable_mode_{VisibilityMode::Hidden};
    std::vector<Spec> children_;
};

} // namespace ui
