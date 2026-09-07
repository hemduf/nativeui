#include "example_support.hpp"

namespace {

ui::LinearGradient spectrum() {
    return ui::LinearGradient{
        {20.0f, 0.0f},
        {470.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {0.95f, 0.20f, 0.15f, 1.0f}},
            ui::GradientStop{0.5f, {0.25f, 0.80f, 0.45f, 1.0f}},
            ui::GradientStop{1.0f, {0.20f, 0.35f, 0.95f, 1.0f}},
        },
    };
}

void draw_scene(ui::CanvasContext2D& g) {
    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
    g.fill_rounded_rect({20.0f, 28.0f, 450.0f, 58.0f}, 14.0f, spectrum());

    const ui::RadialGradient glow{
        {135.0f, 150.0f},
        58.0f,
        {
            ui::GradientStop{0.0f, {1.0f, 0.92f, 0.45f, 1.0f}},
            ui::GradientStop{1.0f, {0.25f, 0.08f, 0.25f, 1.0f}},
        },
    };
    g.fill_rounded_rect({70.0f, 100.0f, 130.0f, 100.0f}, 14.0f, glow);

    const ui::LinearGradient overlay{
        {250.0f, 100.0f}, {430.0f, 200.0f},
        {0.55f, 0.80f, 1.0f, 1.0f}, {1.0f, 0.55f, 0.55f, 1.0f}};
    g.fill_rounded_rect({250.0f, 100.0f, 180.0f, 100.0f}, 14.0f,
                        {0.78f, 0.52f, 0.28f, 1.0f});
    g.fill_rounded_rect({250.0f, 100.0f, 180.0f, 100.0f}, 14.0f,
                        overlay, ui::PaintOptions{0.85f, ui::BlendMode::Multiply});

    g.text({20.0f, 214.0f},
           "linear + radial gradients / multi-stop / opacity / multiply",
           11.0f, ui::colors::textMuted);
}

bool red_dominant(ui::Rgba8 pixel) {
    return pixel.r > 170 && pixel.r > pixel.b * 2;
}

bool blue_dominant(ui::Rgba8 pixel) {
    return pixel.b > 170 && pixel.b > pixel.r * 2;
}

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T021 / GRADIENT PAINTS"},
                ui::Canvas{500.0f, 230.0f, draw_scene}
            }.padding(20.0f).gap(12.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        const ui::LinearGradient linear{
            {0.0f, 0.0f}, {24.0f, 0.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
        const ui::RadialGradient radial{
            {36.0f, 8.0f}, 7.0f,
            {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};

        ui::UI tree{
            ui::Canvas{48.0f, 16.0f, [linear, radial](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 0.0f, 24.0f, 16.0f}, linear);
                g.fill_rect({24.0f, 0.0f, 24.0f, 16.0f}, radial);
            }}
        };
        ui::HeadlessRenderer renderer{{48.0f, 16.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless render failed");
        if (!red_dominant(renderer.pixel(2, 8))) {
            return example::fail("linear gradient start was not rendered");
        }
        if (!blue_dominant(renderer.pixel(21, 8))) {
            return example::fail("linear gradient end was not rendered");
        }
        const auto center = renderer.pixel(36, 8);
        const auto edge = renderer.pixel(42, 8);
        if (!(center.r > edge.r + 100 && center.g > edge.g + 100 && center.b > edge.b + 100)) {
            return example::fail("radial gradient falloff was not rendered");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T021 - Gradient Paints", {580.0f, 400.0f});
}
