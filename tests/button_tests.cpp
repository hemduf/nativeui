#include "test_support.hpp"

#include <cstdlib>

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

bool pixel_near(ui::Rgba8 pixel, ui::Color color, int tolerance = 4) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(value * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

bool same_pixel(ui::Rgba8 a, ui::Rgba8 b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void pointer_and_keyboard_activation() {
    int activations = 0;
    ui::UI tree{ui::Button{"Run", [&activations] { ++activations; }}};

    test::MockPlatform platform;
    tree.resize({180.0f, 64.0f});
    tree.activate(platform);

    // Pointer activation fires only for an armed press released inside.
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(activations == 0);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(activations == 1);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerCancel, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(activations == 1);

    // Space activates on release and ignores repeated KeyDown while held.
    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(activations == 1);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(activations == 2);

    // Enter fires on first KeyDown only until matching KeyUp.
    tree.dispatch(test::key(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 3);
    tree.dispatch(key_up(ui::Key::Enter), platform);
    tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(activations == 4);
}

void availability_and_reentrancy() {
    test::MockPlatform platform;

    // Disabled is the sole activation gate. Disabling while captured cancels
    // the armed interaction before normal input is suppressed.
    {
        ui::State<bool> enabled{true};
        int activations = 0;
        ui::UI tree{
            ui::Enabled{enabled,
                ui::Button{"Apply", [&activations] { ++activations; }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(activations == 0);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Enter), platform) == ui::EventResult::Ignored);
        NUI_CHECK(activations == 0);
    }

    // Hidden has the same interaction cancellation contract without destroying
    // or unmounting the retained Button.
    {
        ui::State<bool> shown{true};
        int activations = 0;
        ui::UI tree{
            ui::Visibility{shown,
                ui::Button{"Hide", [&activations] { ++activations; }}}
                .mode(ui::VisibilityMode::Hidden)
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        shown.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(activations == 0);
    }

    // ReadOnly intentionally does not disable action/command controls.
    {
        ui::State<bool> read_only{true};
        int activations = 0;
        ui::UI tree{
            ui::ReadOnly{read_only,
                ui::Button{"Action", [&activations] { ++activations; }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        tree.dispatch(test::key(ui::Key::Space), platform);
        tree.dispatch(key_up(ui::Key::Space), platform);
        tree.dispatch(test::key(ui::Key::Enter), platform);
        NUI_CHECK(activations == 3);
    }

    // Focus loss cancels a pending Space activation.
    {
        int first_activations = 0;
        int second_activations = 0;
        ui::UI tree{
            ui::Row{
                ui::Button{"First", [&first_activations] { ++first_activations; }},
                ui::Button{"Second", [&second_activations] { ++second_activations; }}
            }.gap(8.0f)
        };
        tree.resize({320.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Space), platform);
        tree.dispatch(test::key(ui::Key::Tab), platform);
        tree.dispatch(key_up(ui::Key::Space), platform);
        NUI_CHECK(first_activations == 0);
        NUI_CHECK(second_activations == 0);
    }

    // User code may synchronously disable the Button from activation. The
    // handler completes without reading Button state after the callback.
    {
        ui::State<bool> enabled{true};
        int activations = 0;
        ui::UI tree{
            ui::Enabled{enabled,
                ui::Button{"One shot", [&] {
                    ++activations;
                    enabled.set(false);
                }}}
        };
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(activations == 1);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
    }

    // Deactivation terminates an armed press without firing.
    {
        int activations = 0;
        ui::UI tree{ui::Button{"Close", [&activations] { ++activations; }}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.deactivate(platform);
        NUI_CHECK(activations == 0);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    }
}

void visual_state_goldens() {
    constexpr ui::Size size{180.0f, 64.0f};
    ui::HeadlessRenderer renderer{size, 1.0f};
    ui::UI tree{ui::Button{"Visual", [] {}}};

    // Solid interior samples are deliberately away from rounded edges/text and
    // therefore form deterministic cross-platform golden checks for each fill.
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), ui::colors::panel));
    const auto normal_edge = renderer.pixel(0, 32);

    test::MockPlatform platform;
    tree.resize(size);
    tree.activate(platform);
    NUI_CHECK(renderer.render(tree));
    const auto focused_edge = renderer.pixel(0, 32);
    NUI_CHECK(!same_pixel(normal_edge, focused_edge));

    tree.dispatch(test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), ui::colors::knob));

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), ui::colors::accent));
    tree.dispatch(test::pointer(ui::InputType::PointerCancel, 20.0f, 20.0f), platform);

    ui::State<bool> enabled{false};
    ui::UI disabled_tree{
        ui::Enabled{enabled, ui::Button{"Disabled", [] {}}}
    };
    NUI_CHECK(renderer.render(disabled_tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), ui::colors::input));
}

void custom_theme_controls_presentation_and_measurement() {
    ui::Theme theme = ui::default_theme();
    theme.palette.surface = ui::Color{0.18f, 0.32f, 0.47f, 1.0f};
    theme.controls.minimum_width = 132.0f;
    theme.controls.control_height = 52.0f;
    theme.typography.control_size = 18.0f;

    ui::UI tree{ui::Button{"Themed control", [] {}}, theme};
    const auto metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.h, 52.0f, 0.0001f);
    NUI_CHECK(metrics.preferred.w >= 132.0f);

    ui::HeadlessRenderer renderer{{180.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), theme.palette.surface));
}

void explicit_style_controls_button_presentation_and_measurement() {
    ui::ButtonStyle style{};
    const ui::Color normal{0.12f, 0.24f, 0.36f, 1.0f};
    const ui::Color hovered{0.22f, 0.34f, 0.46f, 1.0f};
    const ui::Color pressed{0.62f, 0.12f, 0.22f, 1.0f};
    const ui::Color disabled{0.08f, 0.09f, 0.10f, 1.0f};
    style.base.fill = normal;
    style.base.minimum_width = 140.0f;
    style.base.control_height = 48.0f;
    style.hovered.fill = hovered;
    style.pressed.fill = pressed;
    style.disabled.fill = disabled;

    ui::UI tree{ui::Button{"Styled", [] {}}.style(style)};
    const auto metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.h, 48.0f, 0.0001f);
    NUI_CHECK(metrics.preferred.w >= 140.0f);

    constexpr ui::Size size{180.0f, 64.0f};
    ui::HeadlessRenderer renderer{size, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), normal));

    test::MockPlatform platform;
    tree.resize(size);
    tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), hovered));

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), pressed));
    tree.dispatch(test::pointer(ui::InputType::PointerCancel, 20.0f, 20.0f), platform);

    ui::State<bool> enabled{false};
    ui::UI disabled_tree{
        ui::Enabled{enabled, ui::Button{"Disabled style", [] {}}.style(style)}
    };
    NUI_CHECK(renderer.render(disabled_tree));
    NUI_CHECK(pixel_near(renderer.pixel(20, 20), disabled));
}

void suite() {
    pointer_and_keyboard_activation();
    availability_and_reentrancy();
    visual_state_goldens();
    custom_theme_controls_presentation_and_measurement();
    explicit_style_controls_button_presentation_and_measurement();
}

} // namespace

int main() { return test::run("button", &suite); }
