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

void explicit_slider_styles_apply_to_slider_and_range_slider() {
    const ui::Color track{0.12f, 0.61f, 0.27f, 1.0f};
    const ui::Color thumb{0.83f, 0.18f, 0.67f, 1.0f};
    const ui::Color hovered{0.91f, 0.72f, 0.11f, 1.0f};

    ui::SliderStyle style;
    style.base.track = track;
    style.base.active = ui::Color{0.22f, 0.35f, 0.88f, 1.0f};
    style.base.thumb = thumb;
    style.hovered.thumb = hovered;
    style.hovered.active = hovered;

    {
        ui::State<float> value{0.5f};
        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).style(style)};
        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), thumb));
        NUI_CHECK(pixel_matches(renderer.pixel(170, 30), track));

        tree.dispatch(test::pointer(ui::InputType::PointerMove, 100.0f, 30.0f), platform);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(100, 30), hovered));
    }

    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f).style(style)};
        ui::HeadlessRenderer renderer{{200.0f, 60.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(54, 30), thumb));
        NUI_CHECK(pixel_matches(renderer.pixel(170, 30), track));
    }
}

void style_state_invalidation_contract() {
    constexpr ui::Size size{200.0f, 60.0f};

    // Equal effective presentation must not dirty either layout or paint just
    // because the logical hover flag changed.
    {
        ui::State<float> value{0.5f};
        const ui::Color stable{0.31f, 0.47f, 0.63f, 1.0f};
        ui::SliderStyle style;
        style.base.active = stable;
        style.base.thumb = stable;
        style.hovered.active = stable;
        style.hovered.thumb = stable;

        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).style(style)};
        test::MockPlatform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(!tree.paint_dirty());

        tree.dispatch(test::pointer(ui::InputType::PointerMove, 100.0f, 30.0f), platform);
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(!tree.paint_dirty());
    }

    // A visual-state variant that changes measured thumb geometry must request
    // layout + paint rather than only paint.
    {
        ui::State<float> value{0.5f};
        ui::SliderStyle style;
        style.hovered.thumb_diameter = 38.0f;

        ui::UI tree{ui::Slider{value}.range(0.0f, 1.0f).style(style)};
        test::MockPlatform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        NUI_CHECK(renderer.render(tree));

        tree.dispatch(test::pointer(ui::InputType::PointerMove, 100.0f, 30.0f), platform);
        NUI_CHECK(tree.layout_dirty());
        NUI_CHECK(tree.paint_dirty());
    }

    // RangeSlider shares the same typed style contract and must classify the
    // same interaction-driven geometry change identically.
    {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.25f, 0.75f}};
        ui::SliderStyle style;
        style.hovered.focus_ring_width = 8.0f;

        ui::UI tree{ui::RangeSlider{value}.range(0.0f, 1.0f).style(style)};
        test::MockPlatform platform;
        tree.resize(size);
        tree.activate(platform);
        ui::HeadlessRenderer renderer{size, 1.0f};
        NUI_CHECK(renderer.render(tree));

        tree.dispatch(test::pointer(ui::InputType::PointerMove, 100.0f, 30.0f), platform);
        NUI_CHECK(tree.layout_dirty());
        NUI_CHECK(tree.paint_dirty());
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
    explicit_slider_styles_apply_to_slider_and_range_slider();
    style_state_invalidation_contract();
    custom_theme_slider_palette();
    default_theme_preserves_slider_measurement();
}

} // namespace

int main() { return test::run("t032_slider_visual", &suite); }
