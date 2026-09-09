#include "test_support.hpp"

#include <cmath>
#include <limits>
#include <string>

namespace {

void external_invalid_state_is_render_only() {
    {
        ui::State<float> value{std::numeric_limits<float>::quiet_NaN()};
        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).step(0.25f)};
        ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(std::isnan(value.get()));

        value.set(std::numeric_limits<float>::infinity());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(std::isinf(value.get()));
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),
        }};
        ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f).step(0.25f)};
        ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(std::isnan(value.get().low));
        NUI_CHECK(std::isinf(value.get().high));

        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        const auto edited = value.get();
        NUI_CHECK(std::isfinite(edited.low));
        NUI_CHECK(std::isfinite(edited.high));
        NUI_CHECK(edited.low <= edited.high);
        NUI_CHECK(edited.low >= 0.0f && edited.high <= 1.0f);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 100.0f, 30.0f), platform);
    }
}

void observer_reentrancy_does_not_duplicate_widget_writes() {
    {
        ui::State<float> value{0.50f};
        ui::State<bool> enabled{true};
        int writes = 0;
        auto observer = value.observe([&](const float&) {
            ++writes;
            enabled.set(false);
        });
        ui::UI tree{ui::Enabled{enabled, ui::Slider{value}.range(0.0f, 1.0f)}};
        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(value.get(), 0.51f, 0.0001f);
        NUI_CHECK(writes == 1);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(writes == 1);
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::State<bool> enabled{true};
        int writes = 0;
        auto observer = value.observe([&](const ui::RangeValue&) {
            ++writes;
            enabled.set(false);
        });
        ui::UI tree{ui::Enabled{enabled, ui::RangeSlider{value}.range(0.0f, 1.0f)}};
        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(writes == 1);
        NUI_CHECK(value.get().low > 0.25f);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(writes == 1);
    }
}

void t059_hidden_and_collapsed_cancel_capture_once() {
    for (const auto unavailable : {ui::VisibilityMode::Hidden, ui::VisibilityMode::Collapsed}) {
        ui::State<float> value{0.5f};
        ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
        ui::UI tree{ui::Visibility{
            visibility,
            ui::Slider{value}.range(0.0f, 1.0f)
        }};
        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        visibility.set(unavailable);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    }

    ui::State<ui::RangeValue> range{ui::RangeValue{0.25f, 0.75f}};
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    ui::UI tree{ui::Visibility{
        visibility,
        ui::RangeSlider{range}.range(0.0f, 1.0f)
    }};
    test::MockPlatform platform;
    tree.resize({200.0f, 60.0f});
    tree.activate(platform);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    visibility.set(ui::VisibilityMode::Hidden);
    NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
}

void range_slider_nearest_thumb_uses_raw_pointer_position() {
    ui::State<ui::RangeValue> value{ui::RangeValue{0.20f, 0.80f}};
    ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f).step(0.50f)};
    test::MockPlatform platform;
    tree.resize({200.0f, 60.0f});
    tree.activate(platform);

    // The raw pointer value 0.55 is closer to the upper thumb (0.80) than the
    // lower thumb (0.20). Quantizing the pointer to 0.50 before hit selection
    // would manufacture an exact tie and incorrectly choose the lower thumb.
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 110.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(value.get().low, 0.20f, 0.0001f);
    NUI_CHECK_NEAR(value.get().high, 0.50f, 0.0001f);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 110.0f, 30.0f), platform);
}

void formatter_is_display_only_and_uses_effective_value() {
    ui::State<float> value{2.0f};
    int calls = 0;
    float formatted = -1.0f;
    ui::UI tree{ui::Slider{value}
                    .range(0.0f, 1.0f)
                    .formatter([&](float effective) {
                        ++calls;
                        formatted = effective;
                        return std::string{"value"};
                    })};
    ui::HeadlessRenderer renderer{{200.0f, 70.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(calls == 1);
    NUI_CHECK_NEAR(formatted, 1.0f, 0.0001f);
    NUI_CHECK_NEAR(value.get(), 2.0f, 0.0001f);
}

void suite() {
    external_invalid_state_is_render_only();
    observer_reentrancy_does_not_duplicate_widget_writes();
    t059_hidden_and_collapsed_cancel_capture_once();
    range_slider_nearest_thumb_uses_raw_pointer_position();
    formatter_is_display_only_and_uses_effective_value();
}

} // namespace

int main() { return test::run("t032_slider_completion", &suite); }
