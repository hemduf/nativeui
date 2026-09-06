#include "test_support.hpp"

namespace {

void suite() {
    test::MockPlatform platform;
    ui::State<std::string> value{"Init"};
    int submit_count = 0;
    std::string submitted;

    ui::UI tree{
        ui::TextInput{"Name", value}
            .placeholder("Preset")
            .max_length(12)
            .on_submit([&](const std::string& text) {
                ++submit_count;
                submitted = text;
            })
    };

    tree.resize({420.0f, 90.0f});
    tree.activate(platform);
    NUI_CHECK(platform.text_input_active);

    tree.dispatch(test::text(" X"), platform);
    NUI_CHECK(value.get() == "Init X");

    // Select all/copy, cut, and asynchronous paste request.
    tree.dispatch(test::key(ui::Key::A, false, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "Init X");

    tree.dispatch(test::key(ui::Key::X, false, true), platform);
    NUI_CHECK(value.get().empty());
    tree.dispatch(test::key(ui::Key::V, false, true), platform);
    NUI_CHECK(platform.paste_requested);
    NUI_CHECK(platform.paste_request_count == 1);

    // Simulate committed paste text coming back from the platform.
    tree.dispatch(test::text("Pasted"), platform);
    NUI_CHECK(value.get() == "Pasted");

    // Undo/redo use the generic primary shortcut path.
    tree.dispatch(test::key(ui::Key::Z, false, true), platform);
    NUI_CHECK(value.get().empty());
    tree.dispatch(test::key(ui::Key::Z, true, true), platform);
    NUI_CHECK(value.get() == "Pasted");

    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(submit_count == 1);
    NUI_CHECK(submitted == "Pasted");

    // Escape reverts edits performed since the last submit/focus snapshot.
    tree.dispatch(test::text("!"), platform);
    NUI_CHECK(value.get() == "Pasted!");
    tree.dispatch(test::key(ui::Key::Escape), platform);
    NUI_CHECK(value.get() == "Pasted");

    // UTF-8 max length is measured in code points, not bytes.
    ui::State<std::string> unicode{""};
    ui::UI unicode_tree{ui::TextInput{"Unicode", unicode}.max_length(3)};
    unicode_tree.resize({320.0f, 90.0f});
    unicode_tree.activate(platform);
    unicode_tree.dispatch(test::text("éééé"), platform);
    NUI_CHECK(unicode.get() == "ééé");

    // Triple click selects all; copy proves the selection extent.
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 50.0f, 3), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 50.0f, 3), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "Pasted");
}

} // namespace

int main() { return test::run("text_input", &suite); }
