#include <nativeui/visibility.hpp>

namespace ui {

detail::VisibilityComponent::VisibilityComponent(State<VisibilityMode>& state)
    : VisibilityComponent(state.binding()) {}

detail::VisibilityComponent::VisibilityComponent(Binding<VisibilityMode> state)
    : mode_state_(std::move(state)) {}

detail::VisibilityComponent::VisibilityComponent(State<bool>& visible,
                                                 VisibilityMode unavailable_mode)
    : VisibilityComponent(visible.binding(), unavailable_mode) {}

detail::VisibilityComponent::VisibilityComponent(Binding<bool> visible,
                                                 VisibilityMode unavailable_mode)
    : visible_state_(std::move(visible)),
      unavailable_mode_(sanitize_unavailable_mode(unavailable_mode)) {}

ComponentAvailability detail::VisibilityComponent::local_availability() const noexcept {
    ComponentAvailability result{};
    if (mode_state_) {
        result.visibility = mode_state_->get();
    } else if (visible_state_ && !visible_state_->get()) {
        result.visibility = unavailable_mode_;
    }
    return result;
}

void detail::VisibilityComponent::mount(MountContext& context) {
    auto invalidate = context.availability_invalidator();
    if (mode_state_) {
        mode_subscription_ = mode_state_->observe(
            [invalidate](const VisibilityMode&) { invalidate(); });
    } else if (visible_state_) {
        visible_subscription_ = visible_state_->observe(
            [invalidate](const bool&) { invalidate(); });
    }
}

void detail::VisibilityComponent::unmount(LifecycleContext&) {
    mode_subscription_.reset();
    visible_subscription_.reset();
}

VisibilityMode detail::VisibilityComponent::sanitize_unavailable_mode(VisibilityMode mode) noexcept {
    return mode == VisibilityMode::Visible ? VisibilityMode::Hidden : mode;
}

Visibility&& Visibility::mode(VisibilityMode value) && {
    unavailable_mode_ = value == VisibilityMode::Visible ? VisibilityMode::Hidden : value;
    return std::move(*this);
}

Spec Visibility::spec() && {
    const auto unavailable_mode = unavailable_mode_;
    if (mode_state_) {
        auto state = *mode_state_;
        return Spec{
            [state] { return std::make_unique<detail::VisibilityComponent>(state); },
            std::move(children_)};
    }
    auto state = *visible_state_;
    return Spec{
        [state, unavailable_mode] {
            return std::make_unique<detail::VisibilityComponent>(state, unavailable_mode);
        },
        std::move(children_)};
}

} // namespace ui
