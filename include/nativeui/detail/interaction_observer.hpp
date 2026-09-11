#pragma once

#include <nativeui/dispatcher.hpp>
#include <nativeui/paint.hpp>

namespace ui::detail {

class RetainedInteractionObserver {
public:
    virtual ~RetainedInteractionObserver() = default;

    virtual void retained_pointer_hover_changed(bool, Dispatcher) {}
    virtual void retained_focus_within_changed(bool, Dispatcher) {}
};

[[nodiscard]] inline Dispatcher dispatcher_for_platform(PlatformServices& platform) noexcept {
    if (auto* provider = dynamic_cast<DispatcherProvider*>(&platform)) {
        return provider->dispatcher();
    }
    return {};
}

} // namespace ui::detail
