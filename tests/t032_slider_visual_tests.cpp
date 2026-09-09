#include "test_support.hpp"

#include <algorithm>
#include <cmath>

namespace {

bool pixel_matches(ui::Rgba8 pixel, ui::Color color, int tolerance = 3) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

void pure_visual_state_contract() {
    using ui::SliderVisualState;
    using ui::detail::slider_visual_state;

    NUI_CHECK(slider_visual_state(true, false, false, false, false) ==
              SliderVisualState::Normal);
    NUI_CHECK(slider_visual_state(true, false, true, false, false) ==
              SliderVisualState::Hover);
    NUI_CHECK(slider_visual_state(true, false, true, true, false) ==
              SliderVisualState::Pressed);
    NUI_CHECK(slider_visual_state(true, false, false, false, true) ==
              SliderVisualState::Focused);
    NUI_CHECK(slider_visual_state(false, false, true, true, true) ==
              SliderVisualState::Disabled);
    NUI_CHECK(slider_visual_state(true, true, true, true, true) ==
              SliderVisualState::ReadOnly);
}

void deterministic_headless_slider_states() {
    ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};

    {
        ui::State<float> value{0.5f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f)};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::accent));
    }

    {
        ui::State<float> value{0.5f};
        ui::State<bool> read_only{true};
        ui::UI tree{ui::ReadOnly{
            read_only,
            ui::Slider{value}.range(0.0f, 1.0f)
        }};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::track));
    }

    {
        ui::State<float> value{0.5f};
        ui::State<bool> enabled{false};
        ui::UI tree{ui::Enabled{
            enabled,
            ui::Slider{value}.range(0.0f, 1.0f)
        }};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::textMuted));
    }
}

void deterministic_headless_interaction_states() {
    ui::State<float> value{0.5f};
    ui::State<bool> enabled{true};
    ui::UI tree{ui::Enabled{
        enabled,
        ui::Slider{value}.range(0.0f, 1.0f)
    }};
    test::MockPlatform platform;
    tree.resize({200.0f, 60.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};

    // Activation focuses the first focusable control. The focus ring is drawn
    // behind the thumb, so a solid pixel just outside the 7px thumb proves it.
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(108, 30), ui::colors::borderFocus));

    tree.dispatch(test::pointer(ui::InputType::PointerMove, 100.0f, 30.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::caret));

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 100.0f, 30.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::text));

    enabled.set(false);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::textMuted));
    NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
}

void deterministic_headless_orientation_and_two_thumbs() {
    {
        ui::State<float> value{0.5f};
        ui::UI tree{ui::Slider{value}
                        .range(0.0f, 1.0f)
                        .orientation(ui::SliderOrientation::Vertical)};
        ui::HeadlessRenderer renderer{{60.0f, 200.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(30, 100), ui::colors::accent));
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f)};
        ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(54, 30), ui::colors::accent));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), ui::colors::accent));
        NUI_CHECK(pixel_matches(renderer.pixel(146, 30), ui::colors::accent));
    }
}

void suite() {
    pure_visual_state_contract();
    deterministic_headless_slider_states();
    deterministic_headless_interaction_states();
    deterministic_headless_orientation_and_two_thumbs();
}

} // namespace

int main() { return test::run("t032_slider_visual", &suite); }
