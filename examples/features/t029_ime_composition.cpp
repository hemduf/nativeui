#include "example_support.hpp"

namespace {

ui::InputEvent composition(
    ui::CompositionType type,
    std::string text = {},
    std::size_t cursor_byte = 0,
    std::size_t selection_bytes = 0) {
    ui::InputEvent event{};
    event.type = ui::InputType::Composition;
    event.composition.type = type;
    event.composition.text = std::move(text);
    event.composition.cursor_byte = cursor_byte;
    event.composition.selection_bytes = selection_bytes;
    return event;
}

ui::InputEvent committed_text(std::string text) {
    ui::InputEvent event{};
    event.type = ui::InputType::TextInput;
    event.text = std::move(text);
    return event;
}

} // namespace

int main(int argc, char** argv) {
    ui::State<std::string> name{"NativeUI"};
    ui::State<std::string> notes{
        "Focus either editor and use a system IME.\n"
        "Marked text stays transient until the platform commits it."};

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T029 / IME COMPOSITION"},
                ui::TextInput{"Single line", name}.placeholder("Compose text…"),
                ui::TextArea{"Multiline", notes}
                    .placeholder("Compose text on any visible line…")
                    .max_length(4096)
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        example::Platform first_platform;
        example::Platform second_platform;
        ui::State<std::string> first_value{"hello"};
        ui::State<std::string> second_value{"line"};
        ui::UI first{ui::TextInput{"First", first_value}};
        ui::UI second{ui::TextArea{"Second", second_value}};

        first.resize({320.0f, 90.0f});
        second.resize({320.0f, 160.0f});
        first.activate(first_platform);
        second.activate(second_platform);

        first.dispatch(composition(ui::CompositionType::Start), first_platform);
        first.dispatch(
            composition(ui::CompositionType::Update, "日本", 3, 0), first_platform);
        second.dispatch(composition(ui::CompositionType::Start), second_platform);
        second.dispatch(
            composition(ui::CompositionType::Update, "仮", 3, 0), second_platform);

        if (first_value.get() != "hello" || second_value.get() != "line") {
            return example::fail("preedit mutated committed state");
        }

        ui::HeadlessRenderer first_renderer{{320.0f, 90.0f}, 1.0f};
        ui::HeadlessRenderer second_renderer{{320.0f, 160.0f}, 1.0f};
        if (!first_renderer.render(first) || !second_renderer.render(second)) {
            return example::fail("composition render failed");
        }

        first.dispatch(
            composition(ui::CompositionType::Commit, "日本"), first_platform);
        if (first_value.get() != "hello日本" || second_value.get() != "line") {
            return example::fail("composition state leaked between views");
        }

        first.dispatch(committed_text("日本"), first_platform);
        if (first_value.get() != "hello日本") {
            return example::fail("IME commit was inserted twice");
        }

        second.dispatch(composition(ui::CompositionType::Cancel), second_platform);
        if (second_value.get() != "line") {
            return example::fail("cancel changed committed multiline state");
        }

        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T029 - IME composition", {720.0f, 460.0f});
}
