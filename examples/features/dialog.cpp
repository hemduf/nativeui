#include "example_support.hpp"
#include <nativeui/dialog.hpp>

namespace {
ui::AlertDialogSpec alert() {
    return {"Confirmer", "Voulez-vous continuer ?", {
        {"cancel", "Annuler", true, ui::DialogActionRole::Cancel},
        {"ok", "Continuer", true, ui::DialogActionRole::Default}}};
}
int self_test() {
    ui::UI tree{ui::Button{"Document", [] {}}}; example::Platform platform;
    tree.resize({420.0f, 240.0f}); tree.activate(platform);
    ui::Dialog dialog{tree}; int callbacks{};
    if (dialog.show_alert(alert(), [&](ui::DialogResult) { ++callbacks; })
        != ui::DialogShowResult::Shown) return example::fail("alert was not shown");
    ui::HeadlessRenderer renderer{{420.0f, 240.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("alert render failed");
    tree.dispatch(example::key(ui::Key::Escape), platform);
    return callbacks == 1 && !dialog.active() ? 0 : example::fail("alert did not finish once");
}
}
int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::Dialog* controller{};
    ui::UI tree{ui::Padding{20.0f, ui::Button{"Ouvrir le dialogue", [&] {
        if (controller) (void)controller->show_alert(alert(), [](ui::DialogResult) {});
    }}}};
    ui::Dialog dialog{tree}; controller = &dialog;
    return example::run_window(tree, "NativeUI Dialog", {480.0f, 300.0f});
}
