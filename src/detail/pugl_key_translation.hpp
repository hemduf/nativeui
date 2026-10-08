#pragma once

#include <nativeui/input.hpp>
#include <pugl/pugl.h>
#include <cstdint>

namespace ui::detail {

/// Translate all symbolic keys exposed by the pinned Pugl API.
/// Key identity is layout-relative; this is never a text-input decoder.
[[nodiscard]] constexpr Key translate_pugl_key(std::uint32_t key, bool primary) noexcept {
    switch (key) {
    case PUGL_KEY_NONE: return Key::None;
    case PUGL_KEY_BACKSPACE: return Key::Backspace;
    case PUGL_KEY_TAB: return Key::Tab;
    case PUGL_KEY_ENTER: return Key::Enter;
    case PUGL_KEY_ESCAPE: return Key::Escape;
    case PUGL_KEY_DELETE: return Key::Delete;
    case PUGL_KEY_SPACE: return Key::Space;
    case PUGL_KEY_F1: return Key::F1;
    case PUGL_KEY_F2: return Key::F2;
    case PUGL_KEY_F3: return Key::F3;
    case PUGL_KEY_F4: return Key::F4;
    case PUGL_KEY_F5: return Key::F5;
    case PUGL_KEY_F6: return Key::F6;
    case PUGL_KEY_F7: return Key::F7;
    case PUGL_KEY_F8: return Key::F8;
    case PUGL_KEY_F9: return Key::F9;
    case PUGL_KEY_F10: return Key::F10;
    case PUGL_KEY_F11: return Key::F11;
    case PUGL_KEY_F12: return Key::F12;
    case PUGL_KEY_PAGE_UP: return Key::PageUp;
    case PUGL_KEY_PAGE_DOWN: return Key::PageDown;
    case PUGL_KEY_END: return Key::End;
    case PUGL_KEY_HOME: return Key::Home;
    case PUGL_KEY_LEFT: return Key::Left;
    case PUGL_KEY_UP: return Key::Up;
    case PUGL_KEY_RIGHT: return Key::Right;
    case PUGL_KEY_DOWN: return Key::Down;
    case PUGL_KEY_PRINT_SCREEN: return Key::PrintScreen;
    case PUGL_KEY_INSERT: return Key::Insert;
    case PUGL_KEY_PAUSE: return Key::Pause;
    case PUGL_KEY_MENU: return Key::Menu;
    case PUGL_KEY_NUM_LOCK: return Key::NumLock;
    case PUGL_KEY_SCROLL_LOCK: return Key::ScrollLock;
    case PUGL_KEY_CAPS_LOCK: return Key::CapsLock;
    case PUGL_KEY_SHIFT_L: return Key::ShiftLeft;
    case PUGL_KEY_SHIFT_R: return Key::ShiftRight;
    case PUGL_KEY_CTRL_L: return Key::ControlLeft;
    case PUGL_KEY_CTRL_R: return Key::ControlRight;
    case PUGL_KEY_ALT_L: return Key::AltLeft;
    case PUGL_KEY_ALT_R: return Key::AltRight;
    case PUGL_KEY_SUPER_L: return Key::SuperLeft;
    case PUGL_KEY_SUPER_R: return Key::SuperRight;
    case PUGL_KEY_PAD_0: return Key::Pad0;
    case PUGL_KEY_PAD_1: return Key::Pad1;
    case PUGL_KEY_PAD_2: return Key::Pad2;
    case PUGL_KEY_PAD_3: return Key::Pad3;
    case PUGL_KEY_PAD_4: return Key::Pad4;
    case PUGL_KEY_PAD_5: return Key::Pad5;
    case PUGL_KEY_PAD_6: return Key::Pad6;
    case PUGL_KEY_PAD_7: return Key::Pad7;
    case PUGL_KEY_PAD_8: return Key::Pad8;
    case PUGL_KEY_PAD_9: return Key::Pad9;
    case PUGL_KEY_PAD_ENTER: return Key::PadEnter;
    case PUGL_KEY_PAD_PAGE_UP: return Key::PadPageUp;
    case PUGL_KEY_PAD_PAGE_DOWN: return Key::PadPageDown;
    case PUGL_KEY_PAD_END: return Key::PadEnd;
    case PUGL_KEY_PAD_HOME: return Key::PadHome;
    case PUGL_KEY_PAD_LEFT: return Key::PadLeft;
    case PUGL_KEY_PAD_UP: return Key::PadUp;
    case PUGL_KEY_PAD_RIGHT: return Key::PadRight;
    case PUGL_KEY_PAD_DOWN: return Key::PadDown;
    case PUGL_KEY_PAD_CLEAR: return Key::PadClear;
    case PUGL_KEY_PAD_INSERT: return Key::PadInsert;
    case PUGL_KEY_PAD_DELETE: return Key::PadDelete;
    case PUGL_KEY_PAD_EQUAL: return Key::PadEqual;
    case PUGL_KEY_PAD_MULTIPLY: return Key::PadMultiply;
    case PUGL_KEY_PAD_ADD: return Key::PadAdd;
    case PUGL_KEY_PAD_SEPARATOR: return Key::PadSeparator;
    case PUGL_KEY_PAD_SUBTRACT: return Key::PadSubtract;
    case PUGL_KEY_PAD_DECIMAL: return Key::PadDecimal;
    case PUGL_KEY_PAD_DIVIDE: return Key::PadDivide;
    default: return translate_ascii_key(key, primary);
    }
}

} // namespace ui::detail
