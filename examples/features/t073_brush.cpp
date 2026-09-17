#include "example_support.hpp"

#include <utility>

namespace {

ui::Brush panel_brush() {
    return ui::Brush{ui::Color{0.09f, 0.10f, 0.12f, 1.0f}};
}

ui::Brush linear_brush() {
    return ui::Brush{ui::LinearGradient{
        {24.0f, 0.0f},
        {456.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {0.95f, 0.24f, 0.18f, 1.0f}},
            ui::GradientStop{0.5f, {0.24f, 0.82f, 0.48f, 1.0f}},
            ui::GradientStop{1.0f, {0.20f, 0.36f, 0.96f, 1.0f}},
        }}};
}

ui::Brush radial_brush() {
    return ui::Brush{ui::RadialGradient{
        {132.0f, 146.0f},
        54.0f,
        {
            ui::GradientStop{0.0f, {1.0f, 0.95f, 0.58f, 1.0f}},
            ui::GradientStop{0.65f, {0.90f, 0.36f, 0.28f, 1.0f}},
            ui::GradientStop{1.0f, {0.18f, 0.05f, 0.12f, 1.0f}},
        }}};
}

ui::Path make_badge_path() {
    ui::Path path;
    path.move_to({260.0f, 100.0f})
        .line_to({430.0f, 100.0f})
        .line_to({456.0f, 146.0f})
        .line_to({430.0f, 192.0f})
        .line_to({260.0f, 192.0f})
        .line_to({234.0f, 146.0f})
        .close();
    return path;
}

void draw_scene(ui::CanvasContext2D& g) {
    const auto panel = panel_brush();
    const auto linear = linear_brush();
    const auto radial = radial_brush();
    const auto badge = make_badge_path();

    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, panel);
    g.fill_rounded_rect({24.0f, 24.0f, 432.0f, 54.0f}, 14.0f, linear);
    g.circle({132.0f, 146.0f}, 50.0f, radial);
    g.fill_path(badge, linear, ui::PaintOptions{0.90f, ui::BlendMode::SourceOver});
    g.text({24.0f, 216.0f},
           "one Brush value: solid / linear / radial, shared by fill primitives",
           11.0f,
           ui::colors::textMuted);
}

bool red_dominant(ui::Rgba8 pixel) {
    return pixel.r > 150 && pixel.r > pixel.b * 2;
}

bool blue_dominant(ui::Rgba8 pixel) {
    return pixel.b > 150 && pixel.b > pixel.r * 2;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        const ui::Brush solid{ui::Color{1.0f, 0.0f, 0.0f, 1.0f}};
        const ui::Brush radial{ui::RadialGradient{
            {32.0f, 12.0f}, 8.0f,
            {
                ui::GradientStop{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
                ui::GradientStop{1.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
            }}};
        const ui::Brush linear{ui::LinearGradient{
            {48.0f, 0.0f}, {72.0f, 0.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};
        ui::Brush self_moved{ui::Color{0.0f, 1.0f, 0.0f, 1.0f}};
        self_moved = std::move(self_moved);

        ui::Path path;
        path.move_to({48.0f, 2.0f})
            .line_to({72.0f, 2.0f})
            .line_to({72.0f, 22.0f})
            .line_to({48.0f, 22.0f})
            .close();

        ui::UI tree{
            ui::Canvas{72.0f, 24.0f,
                [solid, radial, linear, self_moved, path](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({1.0f, 2.0f, 14.0f, 20.0f}, 3.0f, solid);
                    g.fill_rect({1.0f, 2.0f, 14.0f, 20.0f}, self_moved);
                    g.circle({32.0f, 12.0f}, 8.0f, radial);
                    g.fill_path(path, linear);
                }}
        };
        ui::HeadlessRenderer renderer{{72.0f, 24.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless Brush render failed");
        if (!red_dominant(renderer.pixel(8, 12))) {
            return example::fail("solid or self-moved transparent Brush contract failed");
        }
        const auto center = renderer.pixel(32, 12);
        const auto edge = renderer.pixel(39, 12);
        if (!(center.r > edge.r + 100 && center.g > edge.g + 100 && center.b > edge.b + 100)) {
            return example::fail("radial Brush circle did not render falloff");
        }
        if (!red_dominant(renderer.pixel(50, 12)) || !blue_dominant(renderer.pixel(69, 12))) {
            return example::fail("linear Brush path did not render");
        }
        return 0;
    }

    auto tree = std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T073 / GENERIC BRUSH"},
            ui::Canvas{480.0f, 232.0f, draw_scene}
        }.padding(20.0f).gap(12.0f));
    return example::run_window(*tree, "NativeUI T073 - Generic Brush", {560.0f, 400.0f});
}
