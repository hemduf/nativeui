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

bool region_contains_color(
    ui::HeadlessRenderer& renderer,
    int left,
    int top,
    int right,
    int bottom,
    ui::Color color) {
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (pixel_matches(renderer.pixel(x, y), color)) return true;
        }
    }
    return false;
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

    // Activation focuses the first focusable control. The ring is only 2px
    // thick around the 7px thumb, so a single axis-aligned boundary pixel is
    // antialiased. Require the exact focus color to appear in the local thumb
    // region instead of coupling the contract to one raster edge sample.
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(region_contains_color(
        renderer, 90, 20, 110, 40, ui::colors::borderFocus));

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

void custom_theme_slider_palette() {
    ui::State<float> value{0.5f};
    auto theme = ui::default_theme();
    theme.palette.accent = ui::Color{0.72f, 0.19f, 0.41f, 1.0f};
    ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f), theme};
    ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(100, 30), theme.palette.accent));
}

void default_theme_preserves_slider_measurement() {
    ui::State<float> value{0.5f};

    ui::UI horizontal{ui::Slider{value}.formatter([](float) { return std::string{"0.50"}; })};
    const auto horizontal_metrics = horizontal.measure();
    NUI_CHECK_NEAR(horizontal_metrics.preferred.w, 160.0f, 0.0001f);
    NUI_CHECK_NEAR(horizontal_metrics.preferred.h, 48.0f, 0.0001f);

    ui::UI vertical{
        ui::Slider{value}
            .orientation(ui::SliderOrientation::Vertical)
            .formatter([](float) { return std::string{"0.50"}; })};
    const auto vertical_metrics = vertical.measure();
    NUI_CHECK_NEAR(vertical_metrics.preferred.w, 72.0f, 0.0001f);
    NUI_CHECK_NEAR(vertical_metrics.preferred.h, 160.0f, 0.0001f);
}

void suite() {
    pure_visual_state_contract();
    deterministic_headless_slider_states();
    deterministic_headless_interaction_states();
    deterministic_headless_orientation_and_two_thumbs();
    custom_theme_slider_palette();
    default_theme_preserves_slider_measurement();
}

} // namespace

int main() { return test::run("t032_slider_visual", &suite); }
