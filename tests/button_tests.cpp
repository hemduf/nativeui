#include "test_support.hpp"

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

void pointer_and_keyboard_activation() {
    int activations = 0;
    ui::UI tree{ui::Button{"Run", [&activations] { ++activations; }}};

    test::MockPlatform platform;
    tree.resize({180.0f, 64.0f});
    tree.activate(platform);

    // Pointer activation fires only for an armed press released inside.
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(activations == 0);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(activations == 1);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerCancel, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(activations == 1);

    // Space activates on release and ignores repeated KeyDown while held.
    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(activations == 1);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);

    // Enter fires on first KeyDown only until matching KeyUp.
    tree.dispatch(test::key(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 3);
    tree.dispatch(key_up(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 4);
}

void availability_and_reentrancy() {
    test::MockPlatform platform;

    // Disabled is the sole activation gate. Disabling while captured cancels
    // the armed interaction before normal input is suppressed.
    {
        ui::State<bool> enabled{true};
        int activations = 0;
        ui::UI tree{
            ui::Enabled{enabled,
                ui::Button{"Apply", [&activations] { ++activations; }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(activations == 0);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Enter), platform) == ui::EventResult::Ignored);
        NUI_CHECK(activations == 0);
    }

    // Hidden has the same interaction cancellation contract without destroying
    // or unmounting the retained Button.
    {
        ui::State<bool> shown{true};
        int activations = 0;
        ui::UI tree{
            ui::Visibility{shown,
                ui::Button{"Hide", [&activations] { ++activations; }}}
                .mode(ui::VisibilityMode::Hidden)
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        shown.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(activations == 0);
    }

    // ReadOnly intentionally does not disable action/command controls.
    {
        ui::State<bool> read_only{true};
        int activations = 0;
        ui::UI tree{
            ui::ReadOnly{read_only,
                ui::Button{"Action", [&activations] { ++activations; }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        tree.dispatch(test::key(ui::Key::Space), platform);
        tree.dispatch(key_up(ui::Key::Space), platform);
        tree.dispatch(test::key(ui::Key::Enter), platform);
        NUI_CHECK(activations == 3);
    }

    // Focus loss cancels a pending Space activation.
    {
        int first_activations = 0;
        int second_activations = 0;
        ui::UI tree{
            ui::Row{
                ui::Button{"First", [&first_activations] { ++first_activations; }},
                ui::Button{"Second", [&second_activations] { ++second_activations; }}
            }.gap(8.0f)
        };
        tree.resize({320.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Space), platform);
        tree.dispatch(test::key(ui::Key::Tab), platform);
        tree.dispatch(key_up(ui::Key::Space), platform);
        NUI_CHECK(first_activations == 0);
        NUI_CHECK(second_activations == 0);
    }

    // User code may synchronously disable the Button from activation. The
    // handler completes without reading Button state after the callback.
    {
        ui::State<bool> enabled{true};
        int activations = 0;
        ui::UI tree{
            ui::Enabled{enabled,
                ui::Button{"One shot", [&] {
                    ++activations;
                    enabled.set(false);
                }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(activations == 1);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
    }

    // Deactivation terminates an armed press without firing.
    {
        int activations = 0;
        ui::UI tree{ui::Button{"Close", [&activations] { ++activations; }}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.deactivate(platform);
        NUI_CHECK(activations == 0);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    }
}

void suite() {
    pointer_and_keyboard_activation();
    availability_and_reentrancy();
}

} // namespace

int main() { return test::run("button", &suite); }
