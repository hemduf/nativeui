#include <nativeui/read_only.hpp>

namespace ui {

detail::ReadOnlyComponent::ReadOnlyComponent(State<bool>& state)
    : ReadOnlyComponent(state.binding()) {}

detail::ReadOnlyComponent::ReadOnlyComponent(Binding<bool> state)
    : state_(std::move(state)) {}

ComponentAvailability detail::ReadOnlyComponent::local_availability() const noexcept {
    ComponentAvailability result{};
    // Binding retains a readable last value after State teardown; no raw State borrow.
    result.read_only = state_.get();
    return result;
}

void detail::ReadOnlyComponent::mount(MountContext& context) {
    subscription_ = state_.observe(
        [invalidate = context.availability_invalidator()](const bool&) { invalidate(); });
}

void detail::ReadOnlyComponent::unmount(LifecycleContext&) { subscription_.reset(); }

Spec ReadOnly::spec() && {
    auto state = state_;
    return Spec{
        [state] { return std::make_unique<detail::ReadOnlyComponent>(state); },
        std::move(children_)};
}

} // namespace ui
