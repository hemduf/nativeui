#include "test_support.hpp"
#include "detail/platform/pointer_state.hpp"

namespace {

ui::InputEvent contact(ui::InputType type, ui::PointerId id, float x, float y) {
    ui::InputEvent result{};
    result.type = type;
    result.pointer.id = id;
    result.position = {x, y};
    return result;
}

void pointer_deltas_are_isolated_and_recover() {
    ui::detail::RawPointerTracker first, second;
    auto down_one = contact(ui::InputType::PointerDown, 1U, 10.0f, 20.0f);
    auto down_two = contact(ui::InputType::PointerDown, 2U, 30.0f, 40.0f);
    first.update(down_one);
    first.update(down_two);
    auto move_one = contact(ui::InputType::PointerMove, 1U, 13.0f, 24.0f);
    first.update(move_one);
    NUI_CHECK(move_one.delta.x == 3.0f && move_one.delta.y == 4.0f);
    auto move_two = contact(ui::InputType::PointerMove, 2U, 35.0f, 48.0f);
    first.update(move_two);
    NUI_CHECK(move_two.delta.x == 5.0f && move_two.delta.y == 8.0f);

    auto sibling = contact(ui::InputType::PointerMove, 1U, 18.0f, 29.0f);
    second.update(sibling);
    NUI_CHECK(sibling.delta.x == 0.0f && sibling.delta.y == 0.0f);

    auto up_one = contact(ui::InputType::PointerUp, 1U, 15.0f, 25.0f);
    first.update(up_one);
    NUI_CHECK(up_one.delta.x == 2.0f && up_one.delta.y == 1.0f);
    auto next_two = contact(ui::InputType::PointerMove, 2U, 37.0f, 49.0f);
    first.update(next_two);
    NUI_CHECK(next_two.delta.x == 2.0f && next_two.delta.y == 1.0f);

    auto cancel_two = contact(ui::InputType::PointerCancel, 2U, 37.0f, 49.0f);
    first.update(cancel_two);
    auto reused = contact(ui::InputType::PointerMove, 2U, 300.0f, 400.0f);
    first.update(reused);
    NUI_CHECK(reused.delta.x == 0.0f && reused.delta.y == 0.0f);

    first.clear();
    auto after_focus_loss = contact(ui::InputType::PointerMove, 1U, 30.0f, 40.0f);
    first.update(after_focus_loss);
    NUI_CHECK(after_focus_loss.delta.x == 0.0f && after_focus_loss.delta.y == 0.0f);
}

void bounded_pointer_capacity_is_stable() {
    ui::detail::RawPointerTracker tracker;
    for (ui::PointerId id = 1; id <= 16; ++id) {
        auto down = contact(ui::InputType::PointerDown, id, static_cast<float>(id), 2.0f);
        tracker.update(down);
    }
    auto overflow = contact(ui::InputType::PointerDown, 17U, 1.0f, 1.0f);
    tracker.update(overflow);
    auto overflow_move = contact(ui::InputType::PointerMove, 17U, 5.0f, 6.0f);
    tracker.update(overflow_move);
    NUI_CHECK(overflow_move.delta.x == 0.0f && overflow_move.delta.y == 0.0f);
    auto tracked = contact(ui::InputType::PointerMove, 16U, 20.0f, 5.0f);
    tracker.update(tracked);
    NUI_CHECK(tracked.delta.x == 4.0f && tracked.delta.y == 3.0f);
}

void multi_click_tracker_is_per_view() {
    ui::detail::MultiClickTracker first, second;
    NUI_CHECK(first.count() == 0);
    NUI_CHECK(first.record(1.0, {10.0f, 10.0f}) == 1);
    NUI_CHECK(first.record(1.2, {12.0f, 11.0f}) == 2);
    NUI_CHECK(first.record(1.3, {12.0f, 11.0f}) == 3);
    NUI_CHECK(first.record(1.4, {12.0f, 11.0f}) == 3);
    NUI_CHECK(second.record(1.4, {12.0f, 11.0f}) == 1);
    NUI_CHECK(first.record(2.0, {12.0f, 11.0f}) == 1);
    NUI_CHECK(first.record(2.1, {80.0f, 80.0f}) == 1);
}

} // namespace

int main() {
    return test::run("Pugl view input state", [] {
        pointer_deltas_are_isolated_and_recover();
        bounded_pointer_capacity_is_stable();
        multi_click_tracker_is_per_view();
    });
}
