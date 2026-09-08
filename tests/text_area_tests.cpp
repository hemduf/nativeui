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

    tree.dispatch(test::key(ui::Key::Home), platform);
    tree.dispatch(test::key(ui::Key::Up, true), platform);
    tree.dispatch(test::key(ui::Key::C, false, true), platform);
    NUI_CHECK(platform.clipboard == "abc\n");

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
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{240.0f, 116.0f}, 1.0f};

    ui::InputEvent tick{};
    tick.type = ui::InputType::Tick;
    tree.dispatch(tick, platform);
    NUI_CHECK(renderer.render(tree));
    const auto bottom_view = renderer.rgba_pixels();

    tree.dispatch(test::key(ui::Key::Home, false, true), platform);
    tree.dispatch(tick, platform);
    NUI_CHECK(renderer.render(tree));
    const auto top_view = renderer.rgba_pixels();
    NUI_CHECK(top_view != bottom_view);

    NUI_CHECK(!tree.layout_dirty());
    tree.dispatch(tick, platform);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    const auto caret_view = renderer.rgba_pixels();
    NUI_CHECK(caret_view != top_view);

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

void duplicate_text_delivery_after_ime_commit_is_suppressed() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日本", 6, 0), platform);
    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "one\ntwo日本");

    // Native IME bridges can deliver the just-committed payload again through
    // the ordinary committed-text path. The immediate identical follow-up must
    // be consumed once rather than creating a second edit/history transaction.
    tree.dispatch(test::text("日本"), platform);
    NUI_CHECK(value.get() == "one\ntwo日本");
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

void pointer_edit_cancels_active_composition() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);
    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 50.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 50.0f), platform);

    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "one\ntwo");
}

void preedit_is_visually_distinct_without_committing_state() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};
    tree.resize({320.0f, 180.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{320.0f, 180.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto committed_only = renderer.rgba_pixels();

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.rgba_pixels() != committed_only);
}

void composition_state_is_isolated_between_views() {
    test::MockPlatform first_platform;
    test::MockPlatform second_platform;
    ui::State<std::string> first_value{"first"};
    ui::State<std::string> second_value{"second"};
    ui::UI first{ui::TextArea{"First", first_value}};
    ui::UI second{ui::TextArea{"Second", second_value}};

    first.resize({320.0f, 180.0f});
    second.resize({320.0f, 180.0f});
    first.activate(first_platform);
    second.activate(second_platform);

    first.dispatch(composition(ui::CompositionType::Start), first_platform);
    first.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), first_platform);
    second.dispatch(composition(ui::CompositionType::Start), second_platform);
    second.dispatch(composition(ui::CompositionType::Update, "日", 3, 0), second_platform);

    first.dispatch(composition(ui::CompositionType::Commit, "仮"), first_platform);
    NUI_CHECK(first_value.get() == "first仮");
    NUI_CHECK(second_value.get() == "second");

    second.dispatch(composition(ui::CompositionType::Cancel), second_platform);
    NUI_CHECK(first_value.get() == "first仮");
    NUI_CHECK(second_value.get() == "second");
}

void composition_candidate_tracks_preedit_cursor_on_current_line() {
    test::MockPlatform platform;
    ui::State<std::string> value{"one\ntwo"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({320.0f, 180.0f});
    tree.activate(platform);
    NUI_CHECK_NEAR(platform.text_input_cursor_offset, 34.5f, 0.001f);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日本", 3, 0), platform);
    NUI_CHECK(value.get() == "one\ntwo");

    // The committed caret sits after "two" (3 ASCII bytes). MockPlatform
    // measures each UTF-8 byte as half the 15 px font size, so advancing the
    // preedit cursor through the first 3-byte code point adds 22.5 px. The
    // native candidate offset must follow that transient preedit cursor.
    NUI_CHECK_NEAR(platform.text_input_cursor_offset, 57.0f, 0.001f);
}

void composition_candidate_tracks_visible_line_after_vertical_scroll() {
    test::MockPlatform platform;
    ui::State<std::string> value{"zero\none\ntwo\nthree\nfour\nfive\nsix\nseven"};
    ui::UI tree{ui::TextArea{"Notes", value}};

    tree.resize({240.0f, 116.0f});
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日", 3, 0), platform);
    NUI_CHECK(value.get() == "zero\none\ntwo\nthree\nfour\nfive\nsix\nseven");

    // Eight 22 px lines need 176 px of content height. In this 116 px widget,
    // the text viewport is 76 px high, so the last-line caret scrolls to a
    // visible logical top of y=86. The platform candidate rectangle must
    // describe that visible caret line rather than the whole TextArea field.
    NUI_CHECK_NEAR(platform.text_input_area.y, 86.0f, 0.001f);
    NUI_CHECK_NEAR(platform.text_input_area.h, 22.0f, 0.001f);
}

void suite() {
    enter_and_committed_text_preserve_newlines();
    vertical_navigation_and_selection_cross_lines();
    viewport_scroll_and_caret_rendering();
    synthetic_ime_composition_uses_shared_text_model();
    duplicate_text_delivery_after_ime_commit_is_suppressed();
    focus_loss_cancels_active_composition();
    pointer_edit_cancels_active_composition();
    preedit_is_visually_distinct_without_committing_state();
    composition_state_is_isolated_between_views();
    composition_candidate_tracks_preedit_cursor_on_current_line();
    composition_candidate_tracks_visible_line_after_vertical_scroll();
}

} // namespace

int main() { return test::run("text_area", &suite); }
