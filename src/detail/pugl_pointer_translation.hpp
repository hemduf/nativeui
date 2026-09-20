#pragma once

#include <nativeui/input.hpp>

#include <pugl/pugl.h>

#include <cmath>

namespace ui::detail {

[[nodiscard]] constexpr InputType translate_pugl_pointer_event(
    PuglEventType type) noexcept {
    switch (type) {
    case PUGL_POINTER_DOWN: return InputType::PointerDown;
    case PUGL_POINTER_MOVE: return InputType::PointerMove;
    case PUGL_POINTER_UP: return InputType::PointerUp;
    case PUGL_POINTER_CANCEL: return InputType::PointerCancel;
    default: return InputType::None;
    }
}

[[nodiscard]] constexpr PointerType translate_pugl_pointer_type(
    PuglPointerType type) noexcept {
    switch (type) {
    case PUGL_POINTER_MOUSE: return PointerType::Mouse;
    case PUGL_POINTER_TOUCH: return PointerType::Touch;
    case PUGL_POINTER_PEN: return PointerType::Pen;
    case PUGL_POINTER_ERASER: return PointerType::Eraser;
    case PUGL_POINTER_UNKNOWN: return PointerType::Unknown;
    }
    return PointerType::Unknown;
}

[[nodiscard]] inline PointerContact translate_pugl_pointer_contact(
    const PuglPointerEvent& event,
    float physical_to_logical_scale) noexcept {
    PointerContact contact{};
    contact.id = static_cast<PointerId>(event.id);
    contact.type = translate_pugl_pointer_type(event.pointerType);
    contact.primary = (event.pointerFlags & PUGL_POINTER_IS_PRIMARY) != 0U;
    contact.coalesced = (event.pointerFlags & PUGL_POINTER_IS_COALESCED) != 0U;
    contact.predicted = (event.pointerFlags & PUGL_POINTER_IS_PREDICTED) != 0U;
    contact.pressure = static_cast<float>(event.pressure);

    if (std::isfinite(event.width) && std::isfinite(event.height) &&
        std::isfinite(physical_to_logical_scale) &&
        physical_to_logical_scale > 0.0f) {
        contact.contact_size = {
            static_cast<float>(event.width) / physical_to_logical_scale,
            static_cast<float>(event.height) / physical_to_logical_scale};
    }

    return contact;
}

} // namespace ui::detail
