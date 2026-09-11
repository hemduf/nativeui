#include "example_support.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct DemoState {
    ui::State<int> voice{1};
    int menu_actions{};
};

std::vector<ui::ComboBoxOption<int>> voice_options() {
    return {
        {1, "Mono lead", true},
        {2, "Poly pad", true},
        {3, "Unavailable voice", false},
        {4, "Bass", true},
    };
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T035 — ComboBox + PopupMenu"},
            ui::Label{
                "Both controls use the shared T061 in-view overlay stack. "
                "Keyboard: Down/Enter/Space open, arrows navigate, Enter/Space commit, "
                "Escape cancels and Tab closes before normal traversal."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::ComboBox<int>{state.voice, voice_options()}.placeholder("Choose a voice"),
            ui::PopupMenu{
                "Actions",
                {
                    ui::PopupMenuItem::action("Select poly pad", [&state] {
                        state.voice.set(2);
                        ++state.menu_actions;
                    }),
                    ui::PopupMenuItem::separator(),
                    ui::PopupMenuItem::action("Unavailable action", [] {}, false),
                    ui::PopupMenuItem::action("Select bass", [&state] {
                        state.voice.set(4);
                        ++state.menu_actions;
                    }),
                }},
        }.gap(12.0f).padding(16.0f)};
}

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

int opening_key_contract() {
    {
        example::Platform platform;
        ui::State<int> selection{1};
        ui::UI tree{ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}};
        tree.resize({240.0f, 160.0f});
        tree.activate(platform);

        if (tree.dispatch(example::key(ui::Key::Space), platform) != ui::EventResult::Handled ||
            tree.dispatch(key_up(ui::Key::Space), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
            selection.get() != 1) {
            return example::fail("ComboBox Space open/cancel contract failed");
        }
    }

    {
        example::Platform platform;
        ui::State<int> selection{1};
        ui::UI tree{ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}};
        tree.resize({240.0f, 160.0f});
        tree.activate(platform);

        auto alt_down = example::key(ui::Key::Down);
        alt_down.alt = true;
        if (tree.dispatch(alt_down, platform) != ui::EventResult::Handled ||
            tree.dispatch(key_up(ui::Key::Down), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
            selection.get() != 1) {
            return example::fail("ComboBox Alt+Down open/cancel contract failed");
        }
    }

    {
        example::Platform platform;
        ui::State<int> selection{1};
        ui::UI tree{ui::ComboBox<int>{
            selection,
            {{1, "One", true}, {2, "Disabled", false}, {3, "Three", true}}}};
        tree.resize({240.0f, 160.0f});
        tree.activate(platform);

        if (tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
            tree.dispatch(key_up(ui::Key::Down), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Up), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
            selection.get() != 3) {
            return example::fail("ComboBox Up wrap/disabled-skip contract failed");
        }
        (void)tree.dispatch(key_up(ui::Key::Enter), platform);
    }

    {
        example::Platform platform;
        ui::State<int> selection{1};
        ui::UI tree{ui::Column{
            ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}},
            ui::Button{"After", [] {}},
        }};
        tree.resize({240.0f, 160.0f});
        tree.activate(platform);

        // Dismiss with Tab before the opener's physical KeyUp. That KeyUp is
        // delivered to the next control, so the anchor must clear its retained
        // anti-repeat latch when focus actually leaves after popup teardown.
        if (tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Tab), platform) != ui::EventResult::Handled) {
            return example::fail("ComboBox Tab-before-KeyUp setup failed");
        }
        (void)tree.dispatch(key_up(ui::Key::Down), platform);
        if (tree.dispatch(example::key(ui::Key::Tab, true), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
            tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
            selection.get() != 1) {
            return example::fail("ComboBox stale opener suppression after Tab");
        }
    }

    return 0;
}

int self_test() {
    if (const int result = opening_key_contract(); result != 0) return result;

    example::Platform platform;
    DemoState state;
    auto tree = make_ui(state);
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    // Header/Label are non-focusable, so ComboBox owns initial focus.
    if (tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
        tree.dispatch(key_up(ui::Key::Down), platform) != ui::EventResult::Handled) {
        return example::fail("ComboBox did not open through the shared overlay path");
    }

    // Move from the selected first item to the second enabled item and commit.
    if (tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled ||
        tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        state.voice.get() != 2) {
        return example::fail("ComboBox keyboard navigation/commit failed");
    }
    (void)tree.dispatch(key_up(ui::Key::Enter), platform);

    // Tab reaches PopupMenu after the ComboBox popup has detached/restored focus.
    if (tree.dispatch(example::key(ui::Key::Tab), platform) != ui::EventResult::Handled ||
        tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        tree.dispatch(key_up(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        tree.dispatch(example::key(ui::Key::End), platform) != ui::EventResult::Handled ||
        tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        state.voice.get() != 4 || state.menu_actions != 1) {
        return example::fail("PopupMenu action contract failed");
    }

    ui::HeadlessRenderer renderer{{420.0f, 260.0f}, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail("headless ComboBox/PopupMenu render failed");
    }
    return 0;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(application.last_error().empty()
                                     ? "T035 platform application is invalid"
                                     : application.last_error());
        }

        stage = "standalone";
        DemoState standalone_state;
        auto standalone_ui = make_ui(standalone_state);
        ui::StandaloneWindow standalone{
            application,
            standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T035 platform smoke",
                .size = {520.0f, 320.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(standalone.last_error().empty()
                                     ? "T035 standalone window is invalid"
                                     : standalone.last_error());
        }

        stage = "embedded";
        DemoState embedded_state;
        auto embedded_ui = make_ui(embedded_state);
        ui::EmbeddedView embedded{
            embedded_ui, standalone.native_handle(), {420.0f, 260.0f}};
        if (!embedded.native_handle()) {
            return example::fail(embedded.last_error().empty()
                                     ? "T035 embedded view is invalid"
                                     : embedded.last_error());
        }

        // Platform focus delivery is asynchronous and an embedded passive view
        // is not required to receive keyboard focus merely because it was
        // realized. Prime the retained input contract explicitly with the real
        // native PlatformServices objects rather than a mock service. This keeps
        // focus/capture/text-input teardown bound to the same standalone and
        // embedded views exercised by the smoke.
        standalone_ui.activate(standalone);
        embedded_ui.activate(embedded);

        // Route the same retained input contract while both native renderers are
        // live, then leave each ComboBox popup open for several native paint
        // cycles. This proves T035 uses the ordinary T061 in-view path in both
        // standalone and embedded worlds rather than a native popup window.
        if (standalone_ui.dispatch(example::key(ui::Key::Down), standalone) !=
                ui::EventResult::Handled ||
            standalone_ui.dispatch(key_up(ui::Key::Down), standalone) !=
                ui::EventResult::Handled ||
            embedded_ui.dispatch(example::key(ui::Key::Down), embedded) !=
                ui::EventResult::Handled ||
            embedded_ui.dispatch(key_up(ui::Key::Down), embedded) !=
                ui::EventResult::Handled) {
            return example::fail("T035 native popup open was not handled");
        }

        stage = "native-paint";
        for (int i = 0; i < 8; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());

        stage = "native-dismiss";
        if (standalone_ui.dispatch(example::key(ui::Key::Escape), standalone) !=
                ui::EventResult::Handled ||
            embedded_ui.dispatch(example::key(ui::Key::Escape), embedded) !=
                ui::EventResult::Handled) {
            return example::fail("T035 native popup Escape dismissal failed");
        }
        for (int i = 0; i < 4; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());
        return 0;
    } catch (const std::exception& error) {
        return example::fail(std::string{"T035 platform smoke "} + stage + ": " + error.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") {
        return platform_smoke();
    }

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T035 ComboBox + PopupMenu", {520.0f, 320.0f});
}
