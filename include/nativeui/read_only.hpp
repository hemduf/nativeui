#pragma once

#include <nativeui/detail/availability_wrapper.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

class ReadOnlyComponent final : public AvailabilityWrapperComponent {
public:
    explicit ReadOnlyComponent(State<bool>& state);
    explicit ReadOnlyComponent(Binding<bool> state);

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override;
    void mount(MountContext& context) override;
    void unmount(LifecycleContext&) override;

private:
    Binding<bool> state_;
    Binding<bool>::Subscription subscription_;
};

} // namespace detail

class ReadOnly {
public:
    template <class Child>
    ReadOnly(State<bool>& state, Child&& child)
        : ReadOnly(state.binding(), std::forward<Child>(child)) {}

    template <class Child>
    ReadOnly(Binding<bool> state, Child&& child) : state_(std::move(state)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() &&;

private:
    Binding<bool> state_;
    std::vector<Spec> children_;
};

} // namespace ui
