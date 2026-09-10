#pragma once

#include <nativeui/theme.hpp>

namespace ui::detail {

// Internal per-component borrowed view of the owning tree's Theme. The Theme
// object is owned by Tree and is declared before the retained root, so this
// pointer remains valid for the complete component lifetime. There is no
// process-global mutable theme state.
class ThemeBinding {
public:
    void bind_theme(const Theme& theme) noexcept { theme_ = &theme; }

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

} // namespace ui::detail
