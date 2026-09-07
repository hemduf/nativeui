#include "test_support.hpp"

namespace {

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
    for (int i = 0; i < 24; ++i) {
        document += "line " + std::to_string(i) + " / nativeui textarea";
        if (i != 23) document += '\n';
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

    // Caret blink changes only paint state; it must not require changing the text/layout model.
    tree.dispatch(tick, platform);
    NUI_CHECK(renderer.render(tree));
    const auto caret_view = renderer.rgba_pixels();
    NUI_CHECK(caret_view != top_view);

    // Hide the caret again, then request a wheel scroll. The rendered viewport must advance.
    tree.dispatch(tick, platform);
    ui::InputEvent wheel{};
    wheel.type = ui::InputType::PointerWheel;
    wheel.position = {120.0f, 70.0f};
    wheel.delta = {0.0f, 3.0f};
    tree.dispatch(wheel, platform);
    NUI_CHECK(renderer.render(tree));
    const auto wheel_view = renderer.rgba_pixels();
    NUI_CHECK(wheel_view != top_view);
}

void suite() {
    enter_and_committed_text_preserve_newlines();
    vertical_navigation_and_selection_cross_lines();
    viewport_scroll_and_caret_rendering();
}

} // namespace

int main() { return test::run("text_area", &suite); }
