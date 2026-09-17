#include "example_support.hpp"

namespace {

ui::Brush linear_brush(float width) {
    return ui::Brush{ui::LinearGradient{
        {0.0f, 0.0f},
        {width, 0.0f},
        {
            ui::GradientStop{0.0f, {0.96f, 0.22f, 0.16f, 1.0f}},
            ui::GradientStop{0.5f, {0.24f, 0.82f, 0.48f, 1.0f}},
            ui::GradientStop{1.0f, {0.18f, 0.34f, 0.98f, 1.0f}},
        }}};
}

ui::Path wave_path() {
    ui::Path path;
    path.move_to({24.0f, 188.0f})
        .line_to({132.0f, 126.0f})
        .line_to({240.0f, 188.0f})
        .line_to({348.0f, 126.0f})
        .line_to({456.0f, 188.0f});
    return path;
}

void draw_scene(ui::CanvasContext2D& g) {
    const auto linear = linear_brush(g.width());
    const ui::Brush radial{ui::RadialGradient{
        {368.0f, 112.0f}, 52.0f,
        {
            ui::GradientStop{0.0f, {1.0f, 0.92f, 0.42f, 1.0f}},
            ui::GradientStop{1.0f, {0.18f, 0.34f, 0.98f, 1.0f}},
        }}};
    const auto path = wave_path();

    g.fill_rect({0.0f, 0.0f, g.width(), g.height()}, {0.06f, 0.07f, 0.09f, 1.0f});
    g.stroke_rounded_rect({24.0f, 24.0f, 432.0f, 52.0f}, 14.0f, 5.0f, linear);
    g.line({24.0f, 104.0f}, {240.0f, 104.0f}, 6.0f, linear,
           ui::PaintOptions{0.90f, ui::BlendMode::SourceOver});
    g.arc({368.0f, 112.0f}, 48.0f, -2.6f, 2.6f, 6.0f, radial);
    g.stroke_path(path, linear,
                  ui::StrokeStyle{5.0f, ui::StrokeCap::Round,
                                  ui::StrokeJoin::Round, 4.0f},
                  ui::PaintOptions{0.85f, ui::BlendMode::SourceOver});
    g.text({24.0f, 214.0f},
           "Brush is shared by rounded strokes, lines, arcs and Paths",
           11.0f,
           ui::colors::textMuted);
}

bool red_dominant(ui::Rgba8 pixel) {
    return pixel.r > 150 && pixel.r > pixel.b * 2;
}

bool blue_dominant(ui::Rgba8 pixel) {
    return pixel.b > 150 && pixel.b > pixel.r * 2;
}

bool bright(ui::Rgba8 pixel) {
    return pixel.r > 180 && pixel.g > 180 && pixel.b > 180;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        const ui::Brush linear{ui::LinearGradient{
            {0.0f, 0.0f}, {32.0f, 0.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};
        const ui::Brush radial{ui::RadialGradient{
            {50.0f, 10.0f}, 6.0f,
            {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};
        const ui::Brush solid{ui::Color{1.0f, 1.0f, 1.0f, 1.0f}};
        ui::Path path;
        path.move_to({2.0f, 28.0f}).line_to({16.0f, 18.0f}).line_to({30.0f, 28.0f});

        ui::UI tree{
            ui::Canvas{80.0f, 32.0f, [linear, radial, solid, path](ui::CanvasContext2D& g) {
                g.line({2.0f, 4.0f}, {30.0f, 4.0f}, 4.0f, linear);
                g.arc({50.0f, 10.0f}, 6.0f, 0.0f, 6.28318531f, 3.0f, radial);
                g.stroke_rounded_rect({62.0f, 2.0f, 14.0f, 12.0f}, 3.0f, 2.0f, solid);
                g.stroke_path(path, linear,
                              ui::StrokeStyle{3.0f, ui::StrokeCap::Round,
                                              ui::StrokeJoin::Round, 4.0f},
                              ui::PaintOptions{0.75f, ui::BlendMode::SourceOver});
            }}
        };
        ui::HeadlessRenderer renderer{{80.0f, 32.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless Brush stroke render failed");
        if (!red_dominant(renderer.pixel(4, 4)) || !blue_dominant(renderer.pixel(28, 4))) {
            return example::fail("linear Brush line did not preserve Painter coordinates");
        }
        if (!blue_dominant(renderer.pixel(56, 10))) {
            return example::fail("radial Brush arc did not render");
        }
        if (!bright(renderer.pixel(69, 2))) {
            return example::fail("Brush rounded-rect stroke did not render");
        }
        const auto join = renderer.pixel(16, 18);
        if (!(join.r > 40 && join.b > 40)) {
            return example::fail("Brush Path stroke did not render with PaintOptions");
        }
        return 0;
    }

    auto tree = std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T074 / BRUSH STROKES"},
            ui::Canvas{480.0f, 228.0f, draw_scene}
        }.padding(20.0f).gap(12.0f));
    return example::run_window(*tree, "NativeUI T074 - Brush Strokes", {560.0f, 400.0f});
}
