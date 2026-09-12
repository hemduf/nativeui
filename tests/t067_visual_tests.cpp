#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr ui::Color kHoverSurface{0.145f, 0.155f, 0.175f, 1.0f};

bool pixel_matches(ui::Rgba8 pixel, ui::Color color, int tolerance = 3) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

ui::Color source_over(ui::Color foreground, ui::Color background) {
    const float inverse_alpha = 1.0f - foreground.a;
    return {
        foreground.r * foreground.a + background.r * inverse_alpha,
        foreground.g * foreground.a + background.g * inverse_alpha,
        foreground.b * foreground.a + background.b * inverse_alpha,
        1.0f};
}

void suite() {
    using VirtualState = ui::VirtualListState<int>;

    ui::State<std::optional<int>> selected{std::nullopt};
    VirtualState state{
        selected,
        20.0f,
        [](const VirtualState::Item&) { return ui::Spacer{100.0f, 20.0f}; }};
    NUI_CHECK(state.replace({
        VirtualState::Item{0, "zero"},
        VirtualState::Item{1, "disabled", false},
        VirtualState::Item{2, "two"},
    }));

    ui::UI tree{ui::ListView<int>{state}};
    test::MockPlatform platform;
    tree.resize({100.0f, 60.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{100.0f, 60.0f}, 1.0f};

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!pixel_matches(renderer.pixel(20, 10), kHoverSurface));

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 20.0f, 10.0f), platform);
    NUI_CHECK(!selected.get());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(20, 10), kHoverSurface));

    // Moving onto a disabled logical row clears the prior hover and never
    // paints hover on the disabled row.
    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 20.0f, 30.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!pixel_matches(renderer.pixel(20, 10), kHoverSurface));
    NUI_CHECK(!pixel_matches(renderer.pixel(20, 30), kHoverSurface));

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 20.0f, 50.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(20, 50), kHoverSurface));

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 50.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerUp, 20.0f, 50.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(selected.get() && *selected.get() == 2);
    NUI_CHECK(renderer.render(tree));
    const auto selected_surface = source_over(ui::colors::selection, ui::colors::panel);
    NUI_CHECK(pixel_matches(renderer.pixel(20, 50), selected_surface));

    ui::InputEvent leave{};
    leave.type = ui::InputType::PointerLeave;
    leave.position = {100.0f, 50.0f};
    NUI_CHECK(tree.dispatch(leave, platform) == ui::EventResult::Handled);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(20, 50), selected_surface));

    // T038: the public ListView style recipe must apply identically to the
    // virtualized T067 specialization instead of being silently ignored.
    ui::State<std::optional<int>> styled_selected{0};
    VirtualState styled_state{
        styled_selected,
        20.0f,
        [](const VirtualState::Item&) { return ui::Spacer{100.0f, 20.0f}; }};
    NUI_CHECK(styled_state.replace({
        VirtualState::Item{0, "zero"},
        VirtualState::Item{1, "one"},
    }));

    ui::ListViewStyle virtual_style;
    virtual_style.base.surface_fill = ui::Color{0.03f, 0.07f, 0.11f, 1.0f};
    virtual_style.selected.row_fill = ui::Color{0.74f, 0.12f, 0.22f, 1.0f};
    virtual_style.hovered.row_fill = ui::Color{0.12f, 0.66f, 0.30f, 1.0f};
    virtual_style.pressed.row_fill = ui::Color{0.18f, 0.30f, 0.82f, 1.0f};

    ui::UI styled_tree{ui::ListView<int>{styled_state}.style(virtual_style)};
    test::MockPlatform styled_platform;
    styled_tree.resize({100.0f, 40.0f});
    styled_tree.activate(styled_platform);
    ui::HeadlessRenderer styled_renderer{{100.0f, 40.0f}, 1.0f};

    NUI_CHECK(styled_renderer.render(styled_tree));
    NUI_CHECK(pixel_matches(
        styled_renderer.pixel(20, 10),
        source_over(*virtual_style.selected.row_fill, *virtual_style.base.surface_fill)));

    (void)styled_tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 20.0f, 30.0f), styled_platform);
    NUI_CHECK(styled_renderer.render(styled_tree));
    NUI_CHECK(pixel_matches(
        styled_renderer.pixel(20, 30),
        source_over(*virtual_style.hovered.row_fill, *virtual_style.base.surface_fill)));

    NUI_CHECK(styled_tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f), styled_platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(styled_renderer.render(styled_tree));
    NUI_CHECK(pixel_matches(
        styled_renderer.pixel(20, 30),
        source_over(*virtual_style.pressed.row_fill, *virtual_style.base.surface_fill)));
}

} // namespace

int main() { return test::run("t067_visual", &suite); }
