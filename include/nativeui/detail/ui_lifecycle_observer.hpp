#pragma once

namespace ui::detail {
// Instance-owned UI-thread service hook. Hooks perform terminal cancellation
// without application notifications and may outlive their UI through weak ownership.
class UILifecycleObserver {
public:
    virtual ~UILifecycleObserver() = default;
    virtual void ui_will_deactivate() noexcept = 0;
    virtual void ui_will_teardown() noexcept { ui_will_deactivate(); }
};
} // namespace ui::detail
