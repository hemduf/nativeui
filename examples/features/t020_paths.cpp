#include "example_support.hpp"

namespace {

ui::Path make_fill_path() {
    ui::Path path;
    path.move_to({24.0f, 28.0f})
        .line_to({170.0f, 28.0f})
        .quad_to({205.0f, 28.0f}, {205.0f, 65.0f})
        .cubic_to({205.0f, 125.0f}, {150.0f, 150.0f}, {92.0f, 135.0f})
        .line_to({24.0f, 102.0f})
        .close();
    return path;
}

ui::Path make_stroke_path() {
    ui::Path path;
    path.move_to({260.0f, 115.0f})
        .line_to({320.0f, 42.0f})
        .line_to({380.0f, 115.0f})
        .line_to({440.0f, 42.0f});
    return path;
}

void draw_scene(ui::CanvasContext2D& g) {
    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
    g.fill_path(make_fill_path(), ui::colors::accent);
    g.stroke_path(make_stroke_path(), ui::colors::caret,
                  ui::StrokeStyle{7.0f, ui::StrokeCap::Round, ui::StrokeJoin::Bevel});
    g.text({20.0f, 176.0f},
           "move / line / quad / cubic / close + fill / stroke",
           11.0f, ui::colors::textMuted);
}

bool red(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g < 40 && pixel.b < 40 && pixel.a > 220;
}

bool green(ui::Rgba8 pixel) {
    return pixel.g > 220 && pixel.r < 40 && pixel.b < 40 && pixel.a > 220;
}

} // namespace

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T020 / GENERIC PATHS"},
                ui::Canvas{500.0f, 195.0f, draw_scene}
            }.padding(20.0f).gap(12.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        ui::Path fill;
        fill.move_to({4.0f, 4.0f})
            .line_to({28.0f, 4.0f})
            .quad_to({36.0f, 4.0f}, {36.0f, 12.0f})
            .cubic_to({36.0f, 24.0f}, {28.0f, 28.0f}, {4.0f, 28.0f})
            .close();
        ui::Path stroke;
        stroke.move_to({42.0f, 8.0f}).line_to({58.0f, 24.0f});

        ui::UI tree{
            ui::Canvas{64.0f, 36.0f, [fill, stroke](ui::CanvasContext2D& g) {
                g.fill_path(fill, {1.0f, 0.0f, 0.0f, 1.0f});
                g.stroke_path(stroke, {0.0f, 1.0f, 0.0f, 1.0f},
                              ui::StrokeStyle{6.0f, ui::StrokeCap::Square,
                                              ui::StrokeJoin::Bevel});
            }}
        };
        ui::HeadlessRenderer renderer{{64.0f, 36.0f}, 1.0f};
        if (!renderer.render(tree)) return example::fail("headless render failed");
        if (!red(renderer.pixel(12, 12))) return example::fail("filled path was not rendered");
        if (!green(renderer.pixel(50, 16))) return example::fail("stroked path was not rendered");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T020 - Generic Paths", {580.0f, 365.0f});
}
