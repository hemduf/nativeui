#include "test_support.hpp"

namespace {

void enter_and_committed_text_preserve_newlines() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}.placeholder("Type notes").max_length(64)};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);
    NUI_CHECK(platform.text_input_active);

    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(value.get() == "one\ntwo\n");

    tree.dispatch(test::text("α\nβ"), platform);
    NUI_CHECK(value.get() == "one\ntwo\nα\nβ");
}

void vertical_navigation_and_selection_cross_lines() {
    test::MockPlatform platform;
    ui::State<std::string> value{"abc\ndef"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);

    // Cursor starts at document end. Home moves to the current line start,
    // then Shift+Up extends selection to the same visual column on line one.
    tree.dispatch(test::key(ui::Key::Home), platform);
    tree.dispatch(test::key(ui::Key::Up, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "abc\n");

    // Down without Shift collapses/moves vertically; End remains line-local.
    tree.dispatch(test::key(ui::Key::Down), platform);
    tree.dispatch(test::key(ui::Key::End, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "def");
}

void suite() {
    enter_and_committed_text_preserve_newlines();
    vertical_navigation_and_selection_cross_lines();
}

} // namespace

int main() { return test::run("text_area", &suite); }
