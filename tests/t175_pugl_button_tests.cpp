#include "test_support.hpp"

#include "detail/pugl_button_translation.hpp"

#include <cstdint>
#include <limits>

namespace {

void normalized_pugl_button_contract() {
    using ui::InputType;
    using ui::detail::translate_pugl_button;

    NUI_CHECK(translate_pugl_button(0U, true) == InputType::PointerDown);
    NUI_CHECK(translate_pugl_button(0U, false) == InputType::PointerUp);

    NUI_CHECK(translate_pugl_button(1U, true) == InputType::ContextMenu);
    NUI_CHECK(translate_pugl_button(1U, false) == InputType::None);

    NUI_CHECK(translate_pugl_button(2U, true) == InputType::None);
    NUI_CHECK(translate_pugl_button(2U, false) == InputType::None);
    NUI_CHECK(translate_pugl_button(3U, true) == InputType::None);
    NUI_CHECK(translate_pugl_button(3U, false) == InputType::None);
    NUI_CHECK(
        translate_pugl_button(std::numeric_limits<std::uint32_t>::max(), true) ==
        InputType::None);
}

} // namespace

int main() {
    return test::run("t175 Pugl button translation", [] {
        normalized_pugl_button_contract();
    });
}
