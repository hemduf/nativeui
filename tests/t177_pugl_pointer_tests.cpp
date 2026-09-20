#include "detail/pugl_pointer_translation.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) <= 0.001f;
}

} // namespace

int main() {
    static_assert(
        ui::detail::translate_pugl_pointer_event(PUGL_POINTER_DOWN) ==
        ui::InputType::PointerDown);
    static_assert(
        ui::detail::translate_pugl_pointer_event(PUGL_POINTER_MOVE) ==
        ui::InputType::PointerMove);
    static_assert(
        ui::detail::translate_pugl_pointer_event(PUGL_POINTER_UP) ==
        ui::InputType::PointerUp);
    static_assert(
        ui::detail::translate_pugl_pointer_event(PUGL_POINTER_CANCEL) ==
        ui::InputType::PointerCancel);
    static_assert(
        ui::detail::translate_pugl_pointer_type(PUGL_POINTER_TOUCH) ==
        ui::PointerType::Touch);
    static_assert(
        ui::detail::translate_pugl_pointer_type(PUGL_POINTER_PEN) ==
        ui::PointerType::Pen);
    static_assert(PUGL_POINTER_ID_NONE == 0U);

    PuglPointerEvent source{};
    source.type = PUGL_POINTER_MOVE;
    source.id = 42U;
    source.pointerType = PUGL_POINTER_PEN;
    source.pointerFlags =
        PUGL_POINTER_IS_PRIMARY |
        PUGL_POINTER_IS_COALESCED |
        PUGL_POINTER_IS_PREDICTED;
    source.pressure = 0.75;
    source.width = 24.0;
    source.height = 16.0;

    const auto contact =
        ui::detail::translate_pugl_pointer_contact(source, 2.0f);
    if (contact.id != 42U ||
        contact.type != ui::PointerType::Pen ||
        !contact.primary ||
        !contact.coalesced ||
        !contact.predicted ||
        !near(contact.pressure, 0.75f) ||
        !near(contact.contact_size.w, 12.0f) ||
        !near(contact.contact_size.h, 8.0f)) {
        return EXIT_FAILURE;
    }

    source.width = NAN;
    source.height = NAN;
    const auto unknown_size =
        ui::detail::translate_pugl_pointer_contact(source, 2.0f);
    if (!std::isnan(unknown_size.contact_size.w) ||
        !std::isnan(unknown_size.contact_size.h)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
