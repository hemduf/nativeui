#include "test_support.hpp"

#include <nativeui/gesture.hpp>

namespace {

void suite() {
    {
        ui::DragGesture gesture{4.0f};
        NUI_CHECK(!gesture.active());
        gesture.begin({10.0f, 20.0f});
        NUI_CHECK(gesture.active());
        NUI_CHECK(gesture.phase() == ui::GesturePhase::Pressed);

        auto move = gesture.move({12.0f, 22.0f});
        NUI_CHECK(!move.drag_started);
        NUI_CHECK(!move.dragging);
        NUI_CHECK_NEAR(move.total.x, 2.0f, 0.001f);
        NUI_CHECK_NEAR(move.total.y, 2.0f, 0.001f);

        auto end = gesture.end({12.0f, 22.0f});
        NUI_CHECK(end.clicked);
        NUI_CHECK(end.ended);
        NUI_CHECK(!gesture.active());
    }

    {
        ui::DragGesture gesture{5.0f};
        gesture.begin({0.0f, 0.0f});
        auto before = gesture.move({3.0f, 3.0f});
        NUI_CHECK(!before.drag_started);
        auto threshold = gesture.move({3.0f, 4.0f});
        NUI_CHECK(threshold.drag_started);
        NUI_CHECK(threshold.dragging);
        auto after = gesture.move({9.0f, 4.0f});
        NUI_CHECK(!after.drag_started);
        NUI_CHECK(after.dragging);
        NUI_CHECK_NEAR(after.total.x, 9.0f, 0.001f);
        NUI_CHECK_NEAR(after.total.y, 4.0f, 0.001f);
        auto end = gesture.end({9.0f, 4.0f});
        NUI_CHECK(!end.clicked);
        NUI_CHECK(end.ended);
        NUI_CHECK(!gesture.active());
    }

    {
        ui::DragGesture gesture{0.0f};
        gesture.begin({5.0f, 5.0f});
        auto update = gesture.move({5.0f, 5.0f});
        NUI_CHECK(update.drag_started);
        NUI_CHECK(update.dragging);
        auto cancelled = gesture.cancel();
        NUI_CHECK(cancelled.cancelled);
        NUI_CHECK(cancelled.dragging);
        NUI_CHECK(!gesture.active());
        NUI_CHECK(!gesture.cancel().cancelled);
    }

    {
        const ui::Point delta{12.0f, -9.0f};
        NUI_CHECK_NEAR(ui::drag_axis_delta(delta, ui::DragAxis::Horizontal), 12.0f, 0.001f);
        NUI_CHECK_NEAR(ui::drag_axis_delta(delta, ui::DragAxis::Vertical), -9.0f, 0.001f);
        NUI_CHECK_NEAR(ui::drag_value_delta(delta, ui::DragAxis::Vertical, 0.5f), -4.5f, 0.001f);
        NUI_CHECK_NEAR(ui::drag_value_delta(delta, ui::DragAxis::Vertical, 0.5f, true), 4.5f, 0.001f);
    }

    // Integration: Knob uses the same total-drag semantics rather than
    // accumulating platform move deltas.
    {
        ui::State<float> value{0.5f};
        ui::UI tree{ui::Knob{"Value", value}.range(0.0f, 1.0f)};
        test::MockPlatform platform;
        tree.resize({220.0f, 220.0f});
        tree.activate(platform);

        tree.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 80.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 62.0f), platform);
        NUI_CHECK_NEAR(value.get(), 0.6f, 0.01f);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 44.0f), platform);
        NUI_CHECK_NEAR(value.get(), 0.7f, 0.01f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 80.0f, 44.0f), platform);
    }
}

} // namespace

int main() {
    return test::run("gesture", suite);
}
