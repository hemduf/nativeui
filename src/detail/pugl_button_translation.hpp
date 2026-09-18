#pragma once

#include <nativeui/input.hpp>

#include <cstdint>

namespace ui::detail {

// The pinned Pugl backends expose one normalized button convention to NativeUI:
// 0 = primary, 1 = secondary, 2 = middle. Win32/Emscripten map explicitly,
// X11 swaps its native middle/right ids after converting from 1-based values,
// and Cocoa's NSEvent buttonNumber already uses this ordering.
//
// NativeUI intentionally supports primary press/release and secondary press
// only. Secondary release, middle and extra buttons remain ignored in T175.
[[nodiscard]] constexpr InputType translate_pugl_button(
    std::uint32_t button, bool pressed) noexcept {
    if (button == 0U) return pressed ? InputType::PointerDown : InputType::PointerUp;
    if (button == 1U && pressed) return InputType::ContextMenu;
    return InputType::None;
}

} // namespace ui::detail
