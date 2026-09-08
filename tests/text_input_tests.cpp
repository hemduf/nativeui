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

void synthetic_ime_composition_is_transient_and_single_commit() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "hello");

    tree.dispatch(composition(ui::CompositionType::Update, "日本", 6, 0), platform);
    NUI_CHECK(value.get() == "hello");

    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "hello日本");

    // A committed IME composition is one logical history transaction.
    tree.dispatch(test::key(ui::Key::Z, false, true), platform);
    NUI_CHECK(value.get() == "hello");
}

void ime_commit_is_not_duplicated_by_followup_text_input() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日本", 6, 0), platform);
    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "hello日本");

    // Some native IME paths deliver the same committed payload through their
    // composition callback and then through the ordinary committed-text path.
    // NativeUI must consume that follow-up exactly once instead of appending it.
    tree.dispatch(test::text("日本"), platform);
    NUI_CHECK(value.get() == "hello日本");

    // Suppressing the duplicate must not create an extra history transaction.
    tree.dispatch(test::key(ui::Key::Z, false, true), platform);
    NUI_CHECK(value.get() == "hello");
}

void focus_loss_cancels_active_composition() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);
    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "hello");

    tree.deactivate(platform);
    NUI_CHECK(!platform.text_input_active);
    tree.activate(platform);

    // A native commit arriving after focus loss must be stale and inert.
    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "hello");
}

void pointer_edit_cancels_active_composition() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);
    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "hello");

    // Pointer editing takes ownership of the selection/caret, so it must cancel
    // any transient composition before moving the committed-text cursor.
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 50.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 50.0f), platform);

    // A stale native commit from the cancelled composition must be inert.
    tree.dispatch(composition(ui::CompositionType::Commit, "日本"), platform);
    NUI_CHECK(value.get() == "hello");
}

void preedit_is_visually_distinct_without_committing_state() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{320.0f, 90.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto committed_only = renderer.rgba_pixels();

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "仮", 3, 0), platform);
    NUI_CHECK(value.get() == "hello");

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.rgba_pixels() != committed_only);
}

void composition_candidate_tracks_preedit_cursor() {
    test::MockPlatform platform;
    ui::State<std::string> value{"hello"};
    ui::UI tree{ui::TextInput{"Name", value}};

    tree.resize({320.0f, 90.0f});
    tree.activate(platform);
    NUI_CHECK_NEAR(platform.text_input_cursor_offset, 49.5f, 0.001f);

    tree.dispatch(composition(ui::CompositionType::Start), platform);
    tree.dispatch(composition(ui::CompositionType::Update, "日本", 3, 0), platform);
    NUI_CHECK(value.get() == "hello");

    // MockPlatform measures one UTF-8 byte as half the font size. The candidate
    // cursor must therefore follow the first 3-byte preedit code point, rather
    // than remaining at the committed-text caret.
    NUI_CHECK_NEAR(platform.text_input_cursor_offset, 72.0f, 0.001f);
}

void composition_candidate_geometry_scales_at_platform_boundary() {
    const ui::Rect logical_area{12.5f, 8.0f, 120.0f, 24.0f};
    constexpr float logical_cursor_offset = 37.25f;

    const auto at_1x = ui::detail::scale_text_input_geometry(
        logical_area, logical_cursor_offset, 1.0f);
    NUI_CHECK_NEAR(at_1x.first.x, 12.5f, 0.001f);
    NUI_CHECK_NEAR(at_1x.first.y, 8.0f, 0.001f);
    NUI_CHECK_NEAR(at_1x.first.w, 120.0f, 0.001f);
    NUI_CHECK_NEAR(at_1x.first.h, 24.0f, 0.001f);
    NUI_CHECK_NEAR(at_1x.second, 37.25f, 0.001f);

    const auto at_2x = ui::detail::scale_text_input_geometry(
        logical_area, logical_cursor_offset, 2.0f);
    NUI_CHECK_NEAR(at_2x.first.x, 25.0f, 0.001f);
    NUI_CHECK_NEAR(at_2x.first.y, 16.0f, 0.001f);
    NUI_CHECK_NEAR(at_2x.first.w, 240.0f, 0.001f);
    NUI_CHECK_NEAR(at_2x.first.h, 48.0f, 0.001f);
    NUI_CHECK_NEAR(at_2x.second, 74.5f, 0.001f);

    // The editor-facing geometry remains logical. A scale change affects only
    // the platform conversion and must not feed physical values back into it.
    NUI_CHECK_NEAR(logical_area.x, 12.5f, 0.001f);
    NUI_CHECK_NEAR(logical_cursor_offset, 37.25f, 0.001f);
}

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

    synthetic_ime_composition_is_transient_and_single_commit();
    ime_commit_is_not_duplicated_by_followup_text_input();
    focus_loss_cancels_active_composition();
    pointer_edit_cancels_active_composition();
    preedit_is_visually_distinct_without_committing_state();
    composition_candidate_tracks_preedit_cursor();
    composition_candidate_geometry_scales_at_platform_boundary();
}

} // namespace

int main() { return test::run("text_input", &suite); }
