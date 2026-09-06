#include "example_support.hpp"

namespace {
struct Model {
    ui::State<std::string> text{"NativeUI commands"};
    ui::State<bool> enabled{false};
    int scoped{};
    int global{};
};
}

int main(int argc, char** argv) {
    Model model;

    auto make_ui = [&] {
        auto tree = std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T017 / PORTABLE COMMAND ROUTING"},
                ui::TextInput{"Text", model.text},
                ui::CommandScope{[&](ui::Command command) {
                    if (command == ui::Command::Undo) {
                        ++model.scoped;
                        model.enabled.set(!model.enabled.get());
                        return ui::EventResult::Handled;
                    }
                    return ui::EventResult::Ignored;
                }, ui::Toggle{"Scoped command target", model.enabled}},
                ui::Canvas{520.0f, 70.0f, [&](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 10.0f, ui::colors::panel);
                    g.text({14.0f, 24.0f}, "TextInput owns Copy/Paste. On the toggle, Primary+Z is scoped; Primary+C falls through globally.", 10.0f, ui::colors::textMuted);
                    g.text({14.0f, 52.0f}, "scoped: " + std::to_string(model.scoped) + "  global: " + std::to_string(model.global), 10.0f, ui::colors::textMuted);
                }}
            }.padding(20.0f).gap(12.0f));

        tree->set_command_handler([&](ui::Command command) {
            if (command == ui::Command::Copy) {
                ++model.global;
                return ui::EventResult::Handled;
            }
            return ui::EventResult::Ignored;
        });
        return tree;
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({580.0f, 330.0f});
        tree->activate(platform);

        ui::InputEvent select_all = example::key(ui::Key::A);
        select_all.primary = true;
        ui::InputEvent copy = example::key(ui::Key::C);
        copy.primary = true;
        tree->dispatch(select_all, platform);
        tree->dispatch(copy, platform);
        if (platform.clipboard != "NativeUI commands" || model.global != 0) {
            return example::fail("TextInput did not win command routing");
        }

        tree->dispatch(example::key(ui::Key::Tab), platform);
        ui::InputEvent undo = example::key(ui::Key::Z);
        undo.primary = true;
        tree->dispatch(undo, platform);
        if (model.scoped != 1 || !model.enabled.get()) {
            return example::fail("scoped command handler failed");
        }

        tree->dispatch(copy, platform);
        if (model.global != 1) return example::fail("global command fallback failed");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T017 - Commands", {600.0f, 390.0f});
}
