#include <nativeui/enabled.hpp>

namespace ui {

detail::EnabledComponent::EnabledComponent(State<bool>& state)
    : EnabledComponent(state.binding()) {}

detail::EnabledComponent::EnabledComponent(Binding<bool> state)
    : state_(std::move(state)) {}

ComponentAvailability detail::EnabledComponent::local_availability() const noexcept {
    ComponentAvailability result{};
    // Binding retains a readable last value after State teardown; no raw State borrow.
    result.enabled = state_.get();
    return result;
}

void detail::EnabledComponent::mount(MountContext& context) {
    subscription_ = state_.observe(
        [invalidate = context.availability_invalidator()](const bool&) { invalidate(); });
}

void detail::EnabledComponent::unmount(LifecycleContext&) { subscription_.reset(); }

Spec Enabled::spec() && {
    auto state = state_;
    return Spec{
        [state] { return std::make_unique<detail::EnabledComponent>(state); },
        std::move(children_)};
}

} // namespace ui
