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

void composition_is_transient_until_single_commit() {
    ui::TextEditModel model{"hello"};
    model.select_range(1, 4);

    model.begin_composition();
    NUI_CHECK(model.composition_active());
    NUI_CHECK(model.text() == "hello");

    model.update_composition("é🙂", 1, 5);
    NUI_CHECK(model.text() == "hello");
    NUI_CHECK(model.composition_text() == "é🙂");
    // Both supplied offsets point into UTF-8 sequences. The model must clamp
    // them to code-point boundaries inside the transient preedit payload.
    NUI_CHECK(model.composition_cursor_byte() == 0);
    NUI_CHECK(model.composition_selection_bytes() == 2);

    model.update_composition("Ω", 2, 0);
    NUI_CHECK(model.text() == "hello");
    NUI_CHECK(model.composition_text() == "Ω");
    NUI_CHECK(model.composition_cursor_byte() == 2);
    NUI_CHECK(model.composition_selection_bytes() == 0);

    NUI_CHECK(model.commit_composition("日本"));
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.text() == "h日本o");

    // One IME commit is one logical edit transaction.
    NUI_CHECK(model.undo());
    NUI_CHECK(model.text() == "hello");
    NUI_CHECK(!model.can_undo());
}

void composition_cancel_preserves_committed_text() {
    ui::TextEditModel model{"alpha"};
    model.select_range(1, 4);

    model.begin_composition();
    model.update_composition("仮", 3, 0);
    NUI_CHECK(model.composition_active());
    NUI_CHECK(model.text() == "alpha");

    model.cancel_composition();
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.composition_text().empty());
    NUI_CHECK(model.text() == "alpha");
    NUI_CHECK(model.selection_begin() == 1);
    NUI_CHECK(model.selection_end() == 4);
    NUI_CHECK(!model.can_undo());
}

void external_edit_cancels_active_composition() {
    ui::TextEditModel model{"hello"};
    model.select_range(1, 4);
    model.begin_composition();
    model.update_composition("仮", 3, 0);

    // Ordinary committed editing (typing/paste) must use the same deterministic
    // cancel path before mutating the committed buffer. Otherwise stale preedit
    // state survives until a later IME callback and can target an obsolete range.
    NUI_CHECK(model.insert("X"));
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.composition_text().empty());
    NUI_CHECK(model.text() == "hXo");

    // A stale platform commit after the external edit must not be able to apply.
    NUI_CHECK(!model.commit_composition("日本"));
    NUI_CHECK(model.text() == "hXo");
}

void selection_erase_cancels_active_composition() {
    ui::TextEditModel model{"hello"};
    model.select_range(1, 4);
    model.begin_composition();
    model.update_composition("仮", 3, 0);

    // Selection deletion is another committed edit path. It must first cancel
    // transient preedit state using the same deterministic composition path.
    NUI_CHECK(model.erase_selection());
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.composition_text().empty());
    NUI_CHECK(model.text() == "ho");

    // A stale native commit after the deletion must remain inert.
    NUI_CHECK(!model.commit_composition("日本"));
    NUI_CHECK(model.text() == "ho");
}

void suite() {
    insertion_selection_and_limits();
    unicode_and_word_navigation();
    multiline_navigation_preserves_visual_column();
    deletion_and_history();
    utf8_boundaries_are_never_split();
    composition_is_transient_until_single_commit();
    composition_cancel_preserves_committed_text();
    external_edit_cancels_active_composition();
    selection_erase_cancels_active_composition();
}

} // namespace

int main() { return test::run("text_edit_model", &suite); }
