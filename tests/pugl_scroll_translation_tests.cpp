#include "test_support.hpp"

#include "detail/pugl_scroll_translation.hpp"

#include <cstdint>
#include <limits>

namespace {

// Pugl normalizes positive dy to scroll-up on every backend (pinned pugl:
// mac.m/win.c/x11.c/emscripten_events.c). The ctrl/cmd+wheel zoom gesture
// therefore must keep one sign and one factor on all platforms; a wheel-up
// notch (|dy| = 1.0) zooms in by kMagnifyScrollFactor.
void pugl_scroll_normalization_contract() {
    using ui::InputType;
    using ui::detail::translate_pugl_scroll;

    // Plain scroll keeps PointerWheel and never claims a zoom factor.
    const auto plain = translate_pugl_scroll(false, false, -3.0);
    NUI_CHECK(plain.type == InputType::PointerWheel);
    NUI_CHECK(plain.magnification == 0.0F);

    // ctrl and cmd are equivalent zoom modifiers, with the unified sign.
    const auto ctrl_up = translate_pugl_scroll(true, false, 1.0);
    NUI_CHECK(ctrl_up.type == InputType::Magnify);
    NUI_CHECK_NEAR(ctrl_up.magnification, ui::detail::kMagnifyScrollFactor, 1e-6F);

    const auto cmd_up = translate_pugl_scroll(false, true, 1.0);
    NUI_CHECK(cmd_up.type == InputType::Magnify);
    NUI_CHECK_NEAR(cmd_up.magnification, ui::detail::kMagnifyScrollFactor, 1e-6F);

    const auto ctrl_down = translate_pugl_scroll(true, false, -1.0);
    NUI_CHECK(ctrl_down.type == InputType::Magnify);
    NUI_CHECK_NEAR(ctrl_down.magnification, -ui::detail::kMagnifyScrollFactor, 1e-6F);

    // macOS precise two-finger deltas arrive as fractions of one notch unit
    // (pinned pugl mac.m divides them by 20): the factor scales linearly.
    const auto precise = translate_pugl_scroll(false, true, 0.25);
    NUI_CHECK(precise.type == InputType::Magnify);
    NUI_CHECK_NEAR(precise.magnification, 0.1F * 0.25F, 1e-6F);
}

} // namespace

int main() {
    return test::run("pugl scroll translation", [] {
        pugl_scroll_normalization_contract();
    });
}
