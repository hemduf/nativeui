#pragma once

#include <nativeui/detail/availability_wrapper.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

class EnabledComponent final : public AvailabilityWrapperComponent {
public:
    explicit EnabledComponent(State<bool>& state);
    explicit EnabledComponent(Binding<bool> state);

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override;
    void mount(MountContext& context) override;
    void unmount(LifecycleContext&) override;

private:
    Binding<bool> state_;
    Binding<bool>::Subscription subscription_;
};

} // namespace detail

class Enabled {
public:
    template <class Child>
    Enabled(State<bool>& state, Child&& child)
        : Enabled(state.binding(), std::forward<Child>(child)) {}

    template <class Child>
    Enabled(Binding<bool> state, Child&& child) : state_(std::move(state)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() &&;

private:
    Binding<bool> state_;
    std::vector<Spec> children_;
};

} // namespace ui
