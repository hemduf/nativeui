#pragma once
#include "../view_geometry.hpp"
#include <pugl/pugl.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace ui::detail {
inline constexpr std::uintptr_t kCaretTimerId = 0x4E554943u;
inline constexpr double kCaretBlinkSeconds = 0.5;

[[nodiscard]] inline PuglSpan to_pugl_span(float physical) {
    return static_cast<PuglSpan>(physical_to_pugl_view_span(physical));
}

[[noreturn]] inline void throw_pugl(PuglStatus status, const char* what) {
    throw std::runtime_error(std::string(what) + ": " + puglStrerror(status));
}
} // namespace ui::detail
