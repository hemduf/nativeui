#pragma once

#include <nativeui/theme.hpp>

#include <functional>

namespace ui {

struct ComponentAvailability;

namespace detail {

// Internal per-component borrowed view of the owning tree's effective Theme.
// The pointed-to Theme is owned either by Tree or by a retained lexical scope
// ancestor. Both outlive descendants for their complete mounted lifetime, so
// normal widgets keep a cheap borrowed view with no process-global mutable state.
class ThemeBinding {
public:
    virtual ~ThemeBinding() = default;

    virtual void bind_theme(const Theme& theme) noexcept { theme_ = &theme; }

    // Most components simply pass their inherited theme through to descendants.
    // T039 StyleScope overrides this seam with its instance-owned resolved Theme,
    // allowing Tree ancestry to remain the single source of lexical inheritance.
    [[nodiscard]] virtual const Theme& descendant_theme() const noexcept {
        return current_theme();
    }

    // T039 scopes opt into this retained-tree callback. Ordinary style-aware
    // components intentionally ignore it. The callback is installed/cleared at
    // mount/unmount and receives the already-classified effective theme delta.
    virtual void set_theme_change_invalidator(
        std::function<void(ThemeInvalidation)> callback) {
        (void)callback;
    }

    // T038 availability reconciliation asks style-aware components whether an
    // Enabled/ReadOnly transition changes their fully resolved presentation.
    // The conservative default preserves the historical repaint behavior for
    // families that have not opted into exact presentation classification yet.
    [[nodiscard]] virtual bool availability_change_affects_paint(
        const ComponentAvailability&,
        const ComponentAvailability&) const noexcept {
        return true;
    }

protected:
    [[nodiscard]] const Theme& current_theme() const noexcept {
        if (theme_) return *theme_;
        // Components are normally bound by Tree before measurement/painting.
        // Keep direct/internal component use deterministic without mutable
        // process state.
        static const Theme fallback = default_theme();
        return fallback;
    }

private:
    const Theme* theme_{};
};

} // namespace detail
} // namespace ui
