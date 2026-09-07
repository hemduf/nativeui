#include "test_support.hpp"
#include <nativeui/text_edit.hpp>

namespace {

void insertion_selection_and_limits() {
    ui::TextEditModel model{"hello"};
    NUI_CHECK(model.text() == "hello");
    NUI_CHECK(model.cursor() == 5);
    NUI_CHECK(!model.has_selection());

    model.select_range(1, 4);
    NUI_CHECK(model.selected_text() == "ell");
    NUI_CHECK(model.insert("i"));
    NUI_CHECK(model.text() == "hio");
    NUI_CHECK(model.cursor() == 2);
    NUI_CHECK(!model.has_selection());

    ui::TextEditModel unicode{"", 3};
    NUI_CHECK(unicode.insert("é🙂xZ"));
    NUI_CHECK(unicode.text() == "é🙂x");
    NUI_CHECK(ui::text::codepoint_count(unicode.text()) == 3);
}

void unicode_and_word_navigation() {
    ui::TextEditModel model{"one two élan"};
    model.move_to(model.text().size());

    model.move_left(ui::TextMotion::Codepoint);
    NUI_CHECK(model.selected_text().empty());
    NUI_CHECK(model.cursor() == ui::text::previous_codepoint(model.text(), model.text().size()));

    model.move_to(model.text().size());
    model.move_left(ui::TextMotion::Word);
    NUI_CHECK(model.cursor() == 8); // byte offset of UTF-8 "élan"
    model.move_left(ui::TextMotion::Word);
    NUI_CHECK(model.cursor() == 4); // "two"

    model.move_right(ui::TextMotion::Word, true);
    NUI_CHECK(model.selection_begin() == 4);
    NUI_CHECK(model.selection_end() == 8);

    model.select_word_at(9); // inside the UTF-8 word "élan"
    NUI_CHECK(model.selected_text() == "élan");
}

void multiline_navigation_preserves_visual_column() {
    // Byte layout: "ab\né🙂x\nq"
    // line starts: 0, 3, 11. The second line has 3 Unicode scalars.
    ui::TextEditModel model{"ab\né🙂x\nq"};

    model.move_to(10); // end of the second line, visual column 3
    model.move_up();
    NUI_CHECK(model.cursor() == 2); // first line clamps at visual column 2

    // A clamped vertical move must retain the preferred visual column so the
    // reverse move returns to column 3 rather than the clamped column 2.
    model.move_down();
    NUI_CHECK(model.cursor() == 10);

    model.move_line_start();
    NUI_CHECK(model.cursor() == 3);
    model.move_line_end();
    NUI_CHECK(model.cursor() == 10);

    model.move_up(true);
    NUI_CHECK(model.anchor() == 10);
    NUI_CHECK(model.cursor() == 2);
    NUI_CHECK(model.selected_text() == "\né🙂x");

    model.move_down(true);
    NUI_CHECK(model.anchor() == 10);
    NUI_CHECK(model.cursor() == 10);
    NUI_CHECK(!model.has_selection());
}

void deletion_and_history() {
    ui::TextEditModel model{"alpha beta"};
    model.move_to(model.text().size());
    NUI_CHECK(model.backspace(ui::TextMotion::Word));
    NUI_CHECK(model.text() == "alpha ");
    NUI_CHECK(model.can_undo());

    NUI_CHECK(model.undo());
    NUI_CHECK(model.text() == "alpha beta");
    NUI_CHECK(model.can_redo());

    NUI_CHECK(model.redo());
    NUI_CHECK(model.text() == "alpha ");

    model.select_all();
    NUI_CHECK(model.erase_selection());
    NUI_CHECK(model.text().empty());
    NUI_CHECK(model.undo());
    NUI_CHECK(model.text() == "alpha ");
}

void utf8_boundaries_are_never_split() {
    ui::TextEditModel model{"é🙂x"};
    model.move_to(1); // continuation byte inside é: must clamp to a valid boundary
    NUI_CHECK(model.cursor() == 0);

    model.select_range(1, 5); // both indices are potentially inside code points
    NUI_CHECK(model.selection_begin() == 0);
    NUI_CHECK(model.selection_end() == 2 || model.selection_end() == 6);

    model.move_to(model.text().size());
    NUI_CHECK(model.backspace(ui::TextMotion::Codepoint));
    NUI_CHECK(model.text() == "é🙂");
}

void suite() {
    insertion_selection_and_limits();
    unicode_and_word_navigation();
    multiline_navigation_preserves_visual_column();
    deletion_and_history();
    utf8_boundaries_are_never_split();
}

} // namespace

int main() { return test::run("text_edit_model", &suite); }
