#include "example_support.hpp"

int main(int argc, char** argv) {
    ui::State<bool> scope_active{true};
    ui::State<bool> outside{false};
    ui::State<bool> first{false};
    ui::State<bool> second{false};

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T014 / FOCUS SCOPE"},
                ui::Toggle{"Scope active", scope_active},
                ui::Toggle{"Outside", outside},
                ui::FocusScope{scope_active,
                    ui::Column{
                        ui::Toggle{"Scoped A", first},
                        ui::Toggle{"Scoped B (default)", second}}
                        .padding(8.0f).gap(8.0f)}
                    .trap(true)
                    .default_focus(1),
                ui::Canvas{500.0f, 48.0f, [](ui::CanvasContext2D& g) {
                    g.text({0.0f, 16.0f}, "Tab/Shift+Tab are trapped inside the active scope once focus enters it.",
                           11.0f, ui::colors::textMuted);
                }}
            }.padding(20.0f).gap(10.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({520.0f, 320.0f});
        tree->activate(platform);

        // Initial focus is Scope active; Tab -> Outside; Tab enters scope at default B.
        tree->dispatch(example::key(ui::Key::Tab), platform);
        tree->dispatch(example::key(ui::Key::Tab), platform);
        tree->dispatch(example::key(ui::Key::Space), platform);
        if (!second.get() || first.get()) return example::fail("default scoped focus was not selected");

        // Reverse traversal inside a trapped scope moves to A.
        tree->dispatch(example::key(ui::Key::Tab, true), platform);
        tree->dispatch(example::key(ui::Key::Space), platform);
        if (!first.get()) return example::fail("Shift+Tab did not reverse within focus scope");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T014 - Focus Scopes", {560.0f, 390.0f});
}
