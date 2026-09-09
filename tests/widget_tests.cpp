#include "test_support.hpp"

namespace {

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

void slider_pointer_keyboard_contract() {
    test::MockPlatform platform;

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).step(0.25f)};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 200.0f, 30.0f), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 200.0f, 30.0f), platform);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

        value.set(0.5f);
        tree.dispatch(test::key(ui::Key::Right, true), platform);
        NUI_CHECK_NEAR(value.get(), 0.75f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Left), platform);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Home), platform);
        NUI_CHECK_NEAR(value.get(), 0.0f, 0.0001f);
        tree.dispatch(test::key(ui::Key::End), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
    }

    {
        ui::State<float> value{50.0f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 100.0f)};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK_NEAR(value.get(), 51.0f, 0.0001f);
        tree.dispatch(test::key(ui::Key::Right, true), platform);
        NUI_CHECK_NEAR(value.get(), 51.1f, 0.0001f);
    }

    {
        ui::State<float> value{0.0f};
        ui::UI tree{ui::Slider{value}
                        .range(-1.0f, 1.0f)
                        .orientation(ui::SliderOrientation::Vertical)};
        tree.resize({60.0f, 200.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 0.0f), platform);
        NUI_CHECK_NEAR(value.get(), 1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 30.0f, 200.0f), platform);
        NUI_CHECK_NEAR(value.get(), -1.0f, 0.0001f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 200.0f), platform);
    }
}

void slider_t059_contract() {
    test::MockPlatform platform;

    {
        ui::State<float> value{0.5f};
        ui::State<bool> read_only{true};
        int writes = 0;
        auto observer = value.observe([&](const float&) { ++writes; });
        ui::UI tree{ui::ReadOnly{read_only, ui::Slider{value}.range(0.0f, 1.0f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 180.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get(), 0.5f, 0.0001f);
        NUI_CHECK(writes == 0);
    }

    {
        ui::State<float> value{0.5f};
        ui::State<bool> enabled{true};
        ui::UI tree{ui::Enabled{enabled, ui::Slider{value}.range(0.0f, 1.0f)}};
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Ignored);
    }
}

void suite() {
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
    slider_pointer_keyboard_contract();
    slider_t059_contract();
}

} // namespace

int main() { return test::run("widgets", &suite); }
