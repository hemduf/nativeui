#include "example_support.hpp"

namespace {

struct Model {
    ui::TextEditModel edit{"NativeUI text model"};
};

ui::TextMotion motion_for(const ui::InputEvent& event) {
    if (event.gui) return ui::TextMotion::Document;
    if (event.ctrl || event.alt) return ui::TextMotion::Word;
    return ui::TextMotion::Codepoint;
}

} // namespace

int main(int argc, char** argv) {
    Model model;

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T025 / HEADLESS TEXT EDIT MODEL"},
                ui::Canvas{620.0f, 150.0f, [&](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                    g.text({16.0f, 28.0f}, model.edit.text(), 18.0f, ui::colors::text);
                    g.text({16.0f, 62.0f},
                           "cursor=" + std::to_string(model.edit.cursor()) +
                               " selection=[" + std::to_string(model.edit.selection_begin()) +
                               "," + std::to_string(model.edit.selection_end()) + "]",
                           11.0f,
                           ui::colors::textMuted);
                    g.text({16.0f, 94.0f},
                           "Left/Right move; Shift extends; Ctrl/Option jumps words; Backspace/Delete edit; Space inserts '*'",
                           10.0f,
                           ui::colors::textMuted);
                    g.text({16.0f, 122.0f},
                           "Primary+Z undo, Primary+Shift+Z redo, Primary+A select all",
                           10.0f,
                           ui::colors::textMuted);
                }}.on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    bool changed = false;
                    if (event.type == ui::InputType::Command) {
                        switch (event.command) {
                            case ui::Command::Undo: changed = model.edit.undo(); break;
                            case ui::Command::Redo: changed = model.edit.redo(); break;
                            case ui::Command::SelectAll:
                                model.edit.select_all();
                                changed = true;
                                break;
                            default: break;
                        }
                    } else if (event.type == ui::InputType::KeyDown) {
                        switch (event.key) {
                            case ui::Key::Left:
                                model.edit.move_left(motion_for(event), event.shift);
                                changed = true;
                                break;
                            case ui::Key::Right:
                                model.edit.move_right(motion_for(event), event.shift);
                                changed = true;
                                break;
                            case ui::Key::Home:
                                model.edit.move_to(0, event.shift);
                                changed = true;
                                break;
                            case ui::Key::End:
                                model.edit.move_to(model.edit.text().size(), event.shift);
                                changed = true;
                                break;
                            case ui::Key::Backspace: changed = model.edit.backspace(motion_for(event)); break;
                            case ui::Key::Delete: changed = model.edit.delete_forward(motion_for(event)); break;
                            case ui::Key::Space: changed = model.edit.insert("*"); break;
                            default: break;
                        }
                    }
                    if (changed) ctx.invalidate();
                    return changed ? ui::EventResult::Handled : ui::EventResult::Ignored;
                })
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        ui::TextEditModel edit{"one two"};
        edit.move_left(ui::TextMotion::Word);
        if (edit.cursor() != 4) return example::fail("word navigation failed");
        edit.select_word_at(edit.cursor());
        if (!edit.insert("three")) return example::fail("insert failed");
        if (edit.text() != "one three") return example::fail("selection/insertion state failed");
        if (!edit.undo() || edit.text() != "one two") return example::fail("undo failed");
        if (!edit.redo() || edit.text() != "one three") return example::fail("redo failed");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T025 - Text Edit Model", {680.0f, 280.0f});
}
