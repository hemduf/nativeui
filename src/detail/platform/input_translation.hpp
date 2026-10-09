#pragma once
#include <nativeui/input.hpp>
#include <pugl/pugl.h>
#include <cstdint>

namespace ui::detail {
[[nodiscard]] Key translate_key(std::uint32_t key, PuglMods mods);
void apply_modifiers(InputEvent& out, PuglMods state);
} // namespace ui::detail
