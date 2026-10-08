#include "test_support.hpp"
#include "detail/platform/text_input_state.hpp"

namespace {

void transition_geometry_is_per_view_and_idempotent() {
    ui::detail::ViewTextInputState left, right;
    const ui::Rect logical{4.0f, 6.0f, 100.0f, 18.0f};
    const auto first = left.update(true, logical, 3.0f, 2.0f);
    NUI_CHECK(first.has_value());
    NUI_CHECK(first->active && first->active_changed);
    NUI_CHECK(first->physical_area.x == 8.0f);
    NUI_CHECK(first->physical_area.y == 12.0f);
    NUI_CHECK(first->physical_area.w == 200.0f);
    NUI_CHECK(first->physical_area.h == 36.0f);
    NUI_CHECK(first->physical_cursor_offset == 6.0f);
    NUI_CHECK(!left.update(true, logical, 3.0f, 2.0f).has_value());

    const auto other = right.update(true, logical, 3.0f, 1.0f);
    NUI_CHECK(other.has_value() && other->active_changed);
    NUI_CHECK(other->physical_area.w == 100.0f);
    const auto scaled = left.update(true, logical, 3.0f, 3.0f);
    NUI_CHECK(scaled.has_value() && !scaled->active_changed);
    NUI_CHECK(scaled->physical_area.w == 300.0f);

    const auto disabled = left.update(false, {}, 0.0f, 3.0f);
    NUI_CHECK(disabled.has_value() && disabled->active_changed);
    NUI_CHECK(!disabled->active);
    NUI_CHECK(disabled->physical_area.w == 0.0f);
    NUI_CHECK(!left.update(false, {}, 0.0f, 3.0f).has_value());
    NUI_CHECK(!right.update(true, logical, 3.0f, 1.0f).has_value());
}

} // namespace

int main() {
    return test::run("native view text input state", [] {
        transition_geometry_is_per_view_and_idempotent();
    });
}
