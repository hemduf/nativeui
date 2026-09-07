#include "test_support.hpp"

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

void enter_and_committed_text_preserve_newlines() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}.placeholder("Type notes").max_length(64)};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);
    NUI_CHECK(platform.text_input_active);

    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(value.get() == "one\ntwo\n");

    tree.dispatch(test::text("α\nβ"), platform);
    NUI_CHECK(value.get() == "one\ntwo\nα\nβ");
}

void vertical_navigation_and_selection_cross_lines() {
    test::MockPlatform platform;
    ui::State<std::string> value{"abc\ndef"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);

    // Cursor starts at document end. Home moves to the current line start,
    // then Shift+Up extends selection to the same visual column on line one.
    tree.dispatch(test::key(ui::Key::Home), platform);
    tree.dispatch(test::key(ui::Key::Up, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "abc\n");

    // Down without Shift collapses/moves vertically; End remains line-local.
    tree.dispatch(test::key(ui::Key::Down), platform);
    tree.dispatch(test::key(ui::Key::End, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "def");
}

void viewport_scroll_and_caret_rendering() {
    std::string document;
    for (int i = 0; i < 64; ++i) {
        document += "line " + std::to_string(i) + " / nativeui textarea";
        if (i != 63) document += '\n';
    }

    test::MockPlatform platform;
    ui::State<std::string> value{document};
    ui::UI tree{ui::TextArea{"Notes", value}};
    tree.resize({240.0f, 116.0f});
    tree.activate(platform); // cursor starts at the document end, so the viewport follows it.

    ui::HeadlessRenderer renderer{{240.0f, 116.0f}, 1.0f};

    // Hide the caret so this first comparison isolates automatic viewport scrolling.
    ui::InputEvent tick{};
    tick.type = ui::InputType::Tick;
    tree.dispatch(tick, platform);
    NUI_CHECK(renderer.render(tree));
    const auto bottom_view = renderer.rgba_pixels();

    tree.dispatch(test::key(ui::Key::Home, false, true), platform); // document start
    tree.dispatch(tick, platform); // hide caret again after the navigation event
    NUI_CHECK(renderer.render(tree));
    const auto top_view = renderer.rgba_pixels();
    NUI_CHECK(top_view != bottom_view);

    // A caret blink is paint-only. In particular, a large multiline document
    // must not be marked for layout/rebuild merely because the caret toggled.
    NUI_CHECK(!tree.layout_dirty());
    tree.dispatch(tick, platform);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    const auto caret_view = renderer.rgba_pixels();
    NUI_CHECK(caret_view != top_view);

    // Moving the caret back to the document end must restore the scrolled
    // viewport independently of caret visibility.
    tree.dispatch(test::key(ui::Key::End, false, true), platform);
    tree.dispatch(tick, platform);
    NUI_CHECK(renderer.render(tree));
    const auto bottom_again = renderer.rgba_pixels();
    NUI_CHECK(bottom_again == bottom_view);
}

void synthetic_ime_composition_uses_shared_text_model() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    tree.dispatch(composition(ui::CompositionType::Cancel), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日本", 6, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");
    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "one\ntwo日本");

    tree.dispatch(test::key(ui::Key::Z, false, true), platform);
    NUI_CHECK(value.get() == "one\ntwo");
}

void focus_loss_cancels_active_composition() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);
    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    tree.deactivate(platform);
    NUI_CHECK(!platform.text_input_active);
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "one\ntwo");
}

void suite() {
    enter_and_committed_text_preserve_newlines();
    vertical_navigation_and_selection_cross_lines();
    viewport_scroll_and_caret_rendering();
    synthetic_ime_composition_uses_shared_text_model();
    focus_loss_cancels_active_composition();
}

} // namespace

int main() { return test::run("text_area", &suite); }
