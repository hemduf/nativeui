#include "example_support.hpp"

namespace {

ui::InputEvent committed_text(std::string text) {
    ui::InputEvent event{};
    event.type = ui::InputType::TextInput;
    event.text = std::move(text);
    return event;
}

ui::InputEvent primary_key(ui::Key key, bool shift = false) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyDown;
    event.key = key;
    event.shift = shift;
    event.ctrl = true;
    event.primary = true;
    return event;
}

} // namespace

int main(int argc, char** argv) {
    ui::State<std::string> notes{
        "NativeUI TextArea\n"
        "Multiline editing shares the UTF-8 TextEditModel.\n"
        "Use Enter, arrows, Home/End and Shift-selection."};

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T028 / MULTILINE TEXT AREA"},
                ui::TextArea{"Notes", notes}
                    .placeholder("Type multiple lines…")
                    .max_length(4096)
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        example::Platform platform;
        ui::State<std::string> value{"one\ntwo"};
        ui::UI tree{ui::TextArea{"Notes", value}.max_length(128)};
        tree.resize({320.0f, 140.0f});
        tree.activate(platform);

        ui::InputEvent enter{};
        enter.type = ui::InputType::KeyDown;
        enter.key = ui::Key::Enter;
        if (tree.dispatch(enter, platform) != ui::EventResult::Handled ||
            value.get() != "one\ntwo\n") {
            return example::fail("Enter did not append a newline");
        }

        if (tree.dispatch(committed_text("three"), platform) != ui::EventResult::Handled ||
            value.get() != "one\ntwo\nthree") {
            return example::fail("committed multiline text did not update state");
        }

        tree.dispatch(primary_key(ui::Key::Home), platform);
        ui::InputEvent down{};
        down.type = ui::InputType::KeyDown;
        down.key = ui::Key::Down;
        down.shift = true;
        tree.dispatch(down, platform);

        ui::InputEvent copy{};
        copy.type = ui::InputType::Command;
        copy.command = ui::Command::Copy;
        tree.dispatch(copy, platform);
        if (platform.clipboard.empty()) {
            return example::fail("cross-line selection/copy produced no text");
        }

        ui::HeadlessRenderer renderer{{320.0f, 140.0f}, 1.0f};
        if (!renderer.render(tree) || renderer.rgba_pixels().empty()) {
            return example::fail("TextArea headless render failed");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T028 - TextArea", {700.0f, 420.0f});
}
