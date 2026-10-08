#pragma once

// Native Pugl key and modifier translation.
#include <nativeui/input.hpp>
#include <pugl/pugl.h>
#include "../pugl_button_translation.hpp"
#include "../pugl_pointer_translation.hpp"
#include "../pugl_scroll_translation.hpp"
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
    const bool primary = platform_primary_modifier(mods);
    switch (key) {
    case PUGL_KEY_LEFT: return Key::Left;
    case PUGL_KEY_RIGHT: return Key::Right;
    case PUGL_KEY_UP: return Key::Up;
    case PUGL_KEY_DOWN: return Key::Down;
    case PUGL_KEY_F2: return Key::F2;
    case PUGL_KEY_PAGE_UP: return Key::PageUp;
    case PUGL_KEY_PAGE_DOWN: return Key::PageDown;
    case PUGL_KEY_F3: return Key::F3;
    case PUGL_KEY_MENU: return Key::Menu;
    case PUGL_KEY_F10: return Key::F10;
    case PUGL_KEY_HOME: return Key::Home;
    case PUGL_KEY_END: return Key::End;
    case PUGL_KEY_BACKSPACE: return Key::Backspace;
    case PUGL_KEY_DELETE: return Key::Delete;
    case PUGL_KEY_ENTER: return Key::Enter;
    case PUGL_KEY_ESCAPE: return Key::Escape;
    default: break;
    }

    return detail::translate_ascii_key(key, primary);
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
