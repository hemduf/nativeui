#pragma once

#include <nativeui/dispatcher.hpp>
#include <nativeui/paint.hpp>

namespace ui::detail {

class RetainedInteractionObserver {
public:
    virtual ~RetainedInteractionObserver() = default;

    /// Called when the retained pointer-hover route changes. When
    /// `pointer_interaction_active` is true a pointer button is currently held
    /// anywhere in this tree, so a policy that may only trigger on plain hover
    /// must not arm from this transition.
    virtual void retained_pointer_hover_changed(
        bool, bool /*pointer_interaction_active*/, Dispatcher) {}

    /// Called when the focus-within truth value of this component changes.
    virtual void retained_focus_within_changed(
        bool, bool /*pointer_interaction_active*/, Dispatcher) {}
};

[[nodiscard]] inline Dispatcher dispatcher_for_platform(PlatformServices& platform) noexcept {
    if (auto* provider = dynamic_cast<DispatcherProvider*>(&platform)) {
        return provider->dispatcher();
    }
    return {};
}

} // namespace ui::detail
