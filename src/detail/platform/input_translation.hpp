#pragma once

// Native Pugl key and modifier translation.
#include <nativeui/input.hpp>
#include <pugl/pugl.h>
#include "../pugl_button_translation.hpp"
#include "../pugl_pointer_translation.hpp"
#include "../pugl_scroll_translation.hpp"
#include "../pugl_key_translation.hpp"
namespace ui {
namespace {

[[nodiscard]] bool platform_primary_modifier(PuglMods mods) noexcept {
#if defined(__APPLE__)
    return (mods & PUGL_MOD_SUPER) != 0;
#else
    return (mods & PUGL_MOD_CTRL) != 0;
#endif
}

Key translate_key(uint32_t key, PuglMods mods) {
    return detail::translate_pugl_key(key, platform_primary_modifier(mods));
}

void apply_modifiers(InputEvent& out, PuglMods state) {
    out.shift = (state & PUGL_MOD_SHIFT) != 0;
    out.ctrl = (state & PUGL_MOD_CTRL) != 0;
    out.alt = (state & PUGL_MOD_ALT) != 0;
    out.gui = (state & PUGL_MOD_SUPER) != 0;
    out.primary = platform_primary_modifier(state);
}

} // namespace
} // namespace ui
