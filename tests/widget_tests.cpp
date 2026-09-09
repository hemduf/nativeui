#include "test_support.hpp"

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

void button_pointer_and_keyboard_activation() {
    int activations = 0;
    ui::UI tree{
        ui::Button{"Run", [&activations] { ++activations; }}
    };

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

    // Space is press/release activation. Repeated KeyDown while held must not
    // arm/fire more than once.
    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(activations == 1);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);

    // Enter fires on first KeyDown only until the matching KeyUp clears the
    // repeat guard.
    tree.dispatch(test::key(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 3);
    tree.dispatch(key_up(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 4);
}

void button_availability_and_reentrancy() {
    test::MockPlatform platform;

    // T059 Disabled is the sole activation gate. Disabling while captured must
    // cancel the armed interaction before normal input is suppressed.
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
    // handler must complete without reading Button state after the callback.
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

void value_widgets_respect_effective_read_only_state() {
    ui::State<bool> read_only{true};
    ui::State<float> drive{0.50f};
    ui::State<bool> bypass{false};

    ui::UI tree{
        ui::ReadOnly{read_only,
            ui::Row{
                ui::Knob{"Drive", drive},
                ui::Toggle{"Bypass", bypass},
            }.gap(8.0f)}
    };

    test::MockPlatform platform;
    tree.resize({420.0f, 220.0f});
    tree.activate(platform);

    // Read-only remains focusable/targetable, but value mutations are consumed
    // by the value controls without changing their bound State.
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.50f, 0.0001f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!bypass.get());
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(!bypass.get());

    read_only.set(false);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(bypass.get());

    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.51f, 0.0001f);
}

void suite() {
    button_pointer_and_keyboard_activation();
    button_availability_and_reentrancy();

    ui::State<float> drive{0.50f};
    ui::State<float> tone{0.25f};
    ui::State<bool> bypass{false};

    ui::UI tree{
        ui::Row{
            ui::Knob{"Drive", drive},
            ui::Knob{"Tone", tone},
            ui::Toggle{"Bypass", bypass},
        }.gap(8.0f)
    };

    test::MockPlatform platform;
    tree.resize({720.0f, 220.0f});
    tree.activate(platform);

    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK_NEAR(drive.get(), 0.51f, 0.0001f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Right, true), platform);
    NUI_CHECK(tone.get() > 0.25f);
    NUI_CHECK(tone.get() < 0.26f);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(bypass.get());
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(!bypass.get());

    // Existing layout builders remain constructible and resizable.
    ui::State<bool> enabled{true};
    ui::UI layout_tree{
        ui::Padding{8.0f,
            ui::Stack{
                ui::Column{
                    ui::Header{"Overlay"},
                    ui::Spacer{12.0f},
                    ui::Toggle{"Enabled", enabled},
                }
            }
        }
    };
    layout_tree.resize({320.0f, 240.0f});
    layout_tree.activate(platform);
    NUI_CHECK(layout_tree.dirty());

    value_widgets_respect_effective_read_only_state();
}

} // namespace

int main() { return test::run("widgets", &suite); }
