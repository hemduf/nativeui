#include "test_support.hpp"
#include "detail/pugl_key_translation.hpp"

#include <cstdint>
#include <utility>

namespace {

void symbolic_keys_match_pinned_pugl() {
    constexpr std::pair<std::uint32_t, ui::Key> cases[] = {
        {PUGL_KEY_NONE, ui::Key::None},
        {PUGL_KEY_BACKSPACE, ui::Key::Backspace},
        {PUGL_KEY_TAB, ui::Key::Tab},
        {PUGL_KEY_ENTER, ui::Key::Enter},
        {PUGL_KEY_ESCAPE, ui::Key::Escape},
        {PUGL_KEY_DELETE, ui::Key::Delete},
        {PUGL_KEY_SPACE, ui::Key::Space},
        {PUGL_KEY_F1, ui::Key::F1},
        {PUGL_KEY_F2, ui::Key::F2},
        {PUGL_KEY_F3, ui::Key::F3},
        {PUGL_KEY_F4, ui::Key::F4},
        {PUGL_KEY_F5, ui::Key::F5},
        {PUGL_KEY_F6, ui::Key::F6},
        {PUGL_KEY_F7, ui::Key::F7},
        {PUGL_KEY_F8, ui::Key::F8},
        {PUGL_KEY_F9, ui::Key::F9},
        {PUGL_KEY_F10, ui::Key::F10},
        {PUGL_KEY_F11, ui::Key::F11},
        {PUGL_KEY_F12, ui::Key::F12},
        {PUGL_KEY_PAGE_UP, ui::Key::PageUp},
        {PUGL_KEY_PAGE_DOWN, ui::Key::PageDown},
        {PUGL_KEY_END, ui::Key::End},
        {PUGL_KEY_HOME, ui::Key::Home},
        {PUGL_KEY_LEFT, ui::Key::Left},
        {PUGL_KEY_UP, ui::Key::Up},
        {PUGL_KEY_RIGHT, ui::Key::Right},
        {PUGL_KEY_DOWN, ui::Key::Down},
        {PUGL_KEY_PRINT_SCREEN, ui::Key::PrintScreen},
        {PUGL_KEY_INSERT, ui::Key::Insert},
        {PUGL_KEY_PAUSE, ui::Key::Pause},
        {PUGL_KEY_MENU, ui::Key::Menu},
        {PUGL_KEY_NUM_LOCK, ui::Key::NumLock},
        {PUGL_KEY_SCROLL_LOCK, ui::Key::ScrollLock},
        {PUGL_KEY_CAPS_LOCK, ui::Key::CapsLock},
        {PUGL_KEY_SHIFT_L, ui::Key::ShiftLeft},
        {PUGL_KEY_SHIFT_R, ui::Key::ShiftRight},
        {PUGL_KEY_CTRL_L, ui::Key::ControlLeft},
        {PUGL_KEY_CTRL_R, ui::Key::ControlRight},
        {PUGL_KEY_ALT_L, ui::Key::AltLeft},
        {PUGL_KEY_ALT_R, ui::Key::AltRight},
        {PUGL_KEY_SUPER_L, ui::Key::SuperLeft},
        {PUGL_KEY_SUPER_R, ui::Key::SuperRight},
        {PUGL_KEY_PAD_0, ui::Key::Pad0},
        {PUGL_KEY_PAD_1, ui::Key::Pad1},
        {PUGL_KEY_PAD_2, ui::Key::Pad2},
        {PUGL_KEY_PAD_3, ui::Key::Pad3},
        {PUGL_KEY_PAD_4, ui::Key::Pad4},
        {PUGL_KEY_PAD_5, ui::Key::Pad5},
        {PUGL_KEY_PAD_6, ui::Key::Pad6},
        {PUGL_KEY_PAD_7, ui::Key::Pad7},
        {PUGL_KEY_PAD_8, ui::Key::Pad8},
        {PUGL_KEY_PAD_9, ui::Key::Pad9},
        {PUGL_KEY_PAD_ENTER, ui::Key::PadEnter},
        {PUGL_KEY_PAD_PAGE_UP, ui::Key::PadPageUp},
        {PUGL_KEY_PAD_PAGE_DOWN, ui::Key::PadPageDown},
        {PUGL_KEY_PAD_END, ui::Key::PadEnd},
        {PUGL_KEY_PAD_HOME, ui::Key::PadHome},
        {PUGL_KEY_PAD_LEFT, ui::Key::PadLeft},
        {PUGL_KEY_PAD_UP, ui::Key::PadUp},
        {PUGL_KEY_PAD_RIGHT, ui::Key::PadRight},
        {PUGL_KEY_PAD_DOWN, ui::Key::PadDown},
        {PUGL_KEY_PAD_CLEAR, ui::Key::PadClear},
        {PUGL_KEY_PAD_INSERT, ui::Key::PadInsert},
        {PUGL_KEY_PAD_DELETE, ui::Key::PadDelete},
        {PUGL_KEY_PAD_EQUAL, ui::Key::PadEqual},
        {PUGL_KEY_PAD_MULTIPLY, ui::Key::PadMultiply},
        {PUGL_KEY_PAD_ADD, ui::Key::PadAdd},
        {PUGL_KEY_PAD_SEPARATOR, ui::Key::PadSeparator},
        {PUGL_KEY_PAD_SUBTRACT, ui::Key::PadSubtract},
        {PUGL_KEY_PAD_DECIMAL, ui::Key::PadDecimal},
        {PUGL_KEY_PAD_DIVIDE, ui::Key::PadDivide},
    };
    for (const auto& [code, expected] : cases) {
        NUI_CHECK(ui::detail::translate_pugl_key(code, false) == expected);
        NUI_CHECK(ui::detail::translate_pugl_key(code, true) == expected);
    }
}

void all_printable_ascii_is_supported() {
    constexpr std::pair<char, ui::Key> punctuation[] = {
        {'!', ui::Key::Exclamation},
        {'"', ui::Key::DoubleQuote},
        {'#', ui::Key::Hash},
        {'$', ui::Key::Dollar},
        {'%', ui::Key::Percent},
        {'&', ui::Key::Ampersand},
        {'\'', ui::Key::Apostrophe},
        {'(', ui::Key::LeftParen},
        {')', ui::Key::RightParen},
        {'*', ui::Key::Asterisk},
        {'+', ui::Key::Plus},
        {',', ui::Key::Comma},
        {'-', ui::Key::Minus},
        {'.', ui::Key::Period},
        {'/', ui::Key::Slash},
        {':', ui::Key::Colon},
        {';', ui::Key::Semicolon},
        {'<', ui::Key::LessThan},
        {'=', ui::Key::Equal},
        {'>', ui::Key::GreaterThan},
        {'?', ui::Key::Question},
        {'@', ui::Key::At},
        {'[', ui::Key::LeftBracket},
        {'\\', ui::Key::Backslash},
        {']', ui::Key::RightBracket},
        {'^', ui::Key::Caret},
        {'_', ui::Key::Underscore},
        {'`', ui::Key::Grave},
        {'{', ui::Key::LeftBrace},
        {'|', ui::Key::Pipe},
        {'}', ui::Key::RightBrace},
        {'~', ui::Key::Tilde},
    };
    for (const auto& [ch, expected] : punctuation) {
        NUI_CHECK(ui::detail::translate_pugl_key(
            static_cast<std::uint32_t>(ch), false) == expected);
    }
    for (std::uint32_t code = '0'; code <= '9'; ++code) {
        const auto translated = ui::detail::translate_pugl_key(code, false);
        NUI_CHECK(translated == ui::detail::kAsciiDigitKeys[code - '0']);
    }
    for (std::uint32_t code = 'a'; code <= 'z'; ++code) {
        NUI_CHECK(ui::detail::translate_pugl_key(code, false) != ui::Key::None);
        NUI_CHECK(ui::detail::translate_pugl_key(code - ('a' - 'A'), false) ==
                  ui::detail::translate_pugl_key(code, false));
    }
    for (std::uint32_t code = 0x20U; code <= 0x7EU; ++code) {
        NUI_CHECK(ui::detail::translate_pugl_key(code, false) != ui::Key::None);
    }
}

void special_cases_are_preserved() {
    using ui::Key;
    static_assert(static_cast<int>(Key::Quit) == 19);
    static_assert(static_cast<int>(Key::F10) == 45);
    NUI_CHECK(ui::detail::translate_pugl_key('q', true) == Key::Quit);
    NUI_CHECK(ui::detail::translate_pugl_key('Q', true) == Key::Quit);
    NUI_CHECK(ui::detail::translate_pugl_key('q', false) == Key::Q);
    NUI_CHECK(ui::detail::translate_pugl_key(PUGL_KEY_TAB, true) == Key::Tab);
    NUI_CHECK(ui::detail::translate_pugl_key(0x00E9U, false) == Key::None);
    NUI_CHECK(ui::detail::translate_pugl_key(0xE0FFU, false) == Key::None);
    NUI_CHECK(ui::detail::translate_pugl_key(0U, false) == Key::None);
}

} // namespace

int main() {
    return test::run("complete Pugl key translation", [] {
        symbolic_keys_match_pinned_pugl();
        all_printable_ascii_is_supported();
        special_cases_are_preserved();
    });
}
