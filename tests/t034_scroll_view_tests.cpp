#include "test_support.hpp"

namespace {

ui::InputEvent wheel(float x, float y, float dx, float dy) {
    ui::InputEvent event{};
    event.type = ui::InputType::PointerWheel;
    event.position = {x, y};
    event.delta = {dx, dy};
    return event;
}

void wheel_consumption_uses_scroll_state() {
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK_NEAR(state.max_offset().y, 300.0f, 0.001f);
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 40.0f, 0.001f);

    state.set_offset({0.0f, 300.0f});
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);

    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, -25.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 275.0f, 0.001f);
}

void pointer_pan_is_opt_in() {
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Ignored);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 50.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 50.0f, 20.0f), platform);
    NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);

    ui::ScrollState pannable_state{ui::ScrollAxis::Vertical};
    ui::UI pannable{
        ui::ScrollView{pannable_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)};
    pannable.resize({100.0f, 100.0f});
    pannable.activate(platform);

    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(pannable_state.offset().y, 50.0f, 0.001f);
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerUp, 50.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
}

void t059_availability_contract() {
    test::MockPlatform platform;

    ui::State<bool> read_only{true};
    ui::ScrollState readable_state{ui::ScrollAxis::Vertical};
    ui::UI readable{
        ui::ReadOnly{read_only,
            ui::ScrollView{readable_state, ui::Spacer{100.0f, 400.0f}}}};
    readable.resize({100.0f, 100.0f});
    readable.activate(platform);
    NUI_CHECK(readable.dispatch(wheel(20.0f, 20.0f, 0.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(readable_state.offset().y, 30.0f, 0.001f);

    ui::State<bool> enabled{false};
    ui::ScrollState disabled_state{ui::ScrollAxis::Vertical};
    ui::UI disabled{
        ui::Enabled{enabled,
            ui::ScrollView{disabled_state, ui::Spacer{100.0f, 400.0f}}}};
    disabled.resize({100.0f, 100.0f});
    disabled.activate(platform);
    NUI_CHECK(disabled.dispatch(wheel(20.0f, 20.0f, 0.0f, 30.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(disabled_state.offset().y, 0.0f, 0.001f);
}

void suite() {
    wheel_consumption_uses_scroll_state();
    pointer_pan_is_opt_in();
    t059_availability_contract();
}

} // namespace

int main() { return test::run("t034_scroll_view", &suite); }
