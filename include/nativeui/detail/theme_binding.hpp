#pragma once

#include <nativeui/theme.hpp>

namespace ui {

struct ComponentAvailability;

namespace detail {

// Internal per-component borrowed view of the owning tree's Theme. The Theme
// object is owned by Tree and is declared before the retained root, so this
// pointer remains valid for the complete component lifetime. There is no
// process-global mutable theme state.
class ThemeBinding {
public:
    virtual ~ThemeBinding() = default;

    void bind_theme(const Theme& theme) noexcept { theme_ = &theme; }

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
