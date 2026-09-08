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
    NUI_CHECK(model.cursor() == 8);
    model.move_left(ui::TextMotion::Word);
    NUI_CHECK(model.cursor() == 4);

    model.move_right(ui::TextMotion::Word, true);
    NUI_CHECK(model.selection_begin() == 4);
    NUI_CHECK(model.selection_end() == 8);

    model.select_word_at(9);
    NUI_CHECK(model.selected_text() == "élan");
}

void multiline_navigation_preserves_visual_column() {
    ui::TextEditModel model{"ab\né🙂x\nq"};

    model.move_to(10);
    model.move_up();
    NUI_CHECK(model.cursor() == 2);

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
    model.move_to(1);
    NUI_CHECK(model.cursor() == 0);

    model.select_range(1, 5);
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
    NUI_CHECK(model.composition_cursor_byte() == 0);
    NUI_CHECK(model.composition_selection_bytes() == 2);

    // selection_bytes is a byte length relative to the preedit cursor. It must
    // never extend beyond the remaining payload, even when the native side
    // provides an oversized but otherwise boundary-aligned selection length.
    model.update_composition("日本", 3, 99);
    NUI_CHECK(model.composition_cursor_byte() == 3);
    NUI_CHECK(model.composition_selection_bytes() == 3);

    model.update_composition("Ω", 2, 0);
    NUI_CHECK(model.text() == "hello");
    NUI_CHECK(model.composition_text() == "Ω");
    NUI_CHECK(model.composition_cursor_byte() == 2);
    NUI_CHECK(model.composition_selection_bytes() == 0);

    NUI_CHECK(model.commit_composition("日本"));
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.text() == "h日本o");

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

    NUI_CHECK(model.insert("X"));
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.composition_text().empty());
    NUI_CHECK(model.text() == "hXo");

    NUI_CHECK(!model.commit_composition("日本"));
    NUI_CHECK(model.text() == "hXo");
}

void selection_erase_cancels_active_composition() {
    ui::TextEditModel model{"hello"};
    model.select_range(1, 4);
    model.begin_composition();
    model.update_composition("仮", 3, 0);

    NUI_CHECK(model.erase_selection());
    NUI_CHECK(!model.composition_active());
    NUI_CHECK(model.composition_text().empty());
    NUI_CHECK(model.text() == "ho");

    NUI_CHECK(!model.commit_composition("日本"));
    NUI_CHECK(model.text() == "ho");
}

void directional_deletion_cancels_active_composition() {
    ui::TextEditModel backspace{"hello"};
    backspace.move_to(4);
    backspace.begin_composition();
    backspace.update_composition("仮", 3, 0);

    NUI_CHECK(backspace.backspace());
    NUI_CHECK(!backspace.composition_active());
    NUI_CHECK(backspace.composition_text().empty());
    NUI_CHECK(backspace.text() == "helo");
    NUI_CHECK(!backspace.commit_composition("日本"));
    NUI_CHECK(backspace.text() == "helo");

    ui::TextEditModel forward_delete{"hello"};
    forward_delete.move_to(1);
    forward_delete.begin_composition();
    forward_delete.update_composition("仮", 3, 0);

    NUI_CHECK(forward_delete.delete_forward());
    NUI_CHECK(!forward_delete.composition_active());
    NUI_CHECK(forward_delete.composition_text().empty());
    NUI_CHECK(forward_delete.text() == "hllo");
    NUI_CHECK(!forward_delete.commit_composition("日本"));
    NUI_CHECK(forward_delete.text() == "hllo");
}

void history_navigation_cancels_active_composition() {
    ui::TextEditModel undo_model{"hello"};
    NUI_CHECK(undo_model.insert("!"));
    undo_model.begin_composition();
    undo_model.update_composition("仮", 3, 0);

    NUI_CHECK(undo_model.undo());
    NUI_CHECK(!undo_model.composition_active());
    NUI_CHECK(undo_model.composition_text().empty());
    NUI_CHECK(undo_model.text() == "hello");
    NUI_CHECK(!undo_model.commit_composition("日本"));
    NUI_CHECK(undo_model.text() == "hello");

    ui::TextEditModel redo_model{"hello"};
    NUI_CHECK(redo_model.insert("!"));
    NUI_CHECK(redo_model.undo());
    redo_model.begin_composition();
    redo_model.update_composition("仮", 3, 0);

    NUI_CHECK(redo_model.redo());
    NUI_CHECK(!redo_model.composition_active());
    NUI_CHECK(redo_model.composition_text().empty());
    NUI_CHECK(redo_model.text() == "hello!");
    NUI_CHECK(!redo_model.commit_composition("日本"));
    NUI_CHECK(redo_model.text() == "hello!");
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
    directional_deletion_cancels_active_composition();
    history_navigation_cancels_active_composition();
}

} // namespace

int main() { return test::run("text_edit_model", &suite); }
