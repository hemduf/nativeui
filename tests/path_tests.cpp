#include "test_support.hpp"

namespace {

bool red(ui::Rgba8 p) {
    return p.r > 220 && p.g < 50 && p.b < 50 && p.a > 220;
}

bool green(ui::Rgba8 p) {
    return p.g > 220 && p.r < 50 && p.b < 50 && p.a > 220;
}

void suite() {
    // The public builder is backend-neutral and supports every required segment.
    ui::Path filled;
    filled.move_to({10.0f, 10.0f})
          .line_to({48.0f, 10.0f})
          .quad_to({62.0f, 10.0f}, {62.0f, 24.0f})
          .cubic_to({62.0f, 42.0f}, {48.0f, 50.0f}, {30.0f, 50.0f})
          .line_to({10.0f, 38.0f})
          .close();

    ui::Path stroked;
    stroked.move_to({8.0f, 70.0f})
           .line_to({30.0f, 58.0f})
           .line_to({52.0f, 70.0f});

    ui::UI tree{
        ui::Canvas{80.0f, 90.0f, [filled, stroked](ui::CanvasContext2D& g) {
            g.fill_path(filled, {1.0f, 0.0f, 0.0f, 1.0f});
            g.stroke_path(stroked,
                          {0.0f, 1.0f, 0.0f, 1.0f},
                          ui::StrokeStyle{6.0f, ui::StrokeCap::Square,
                                          ui::StrokeJoin::Bevel});
        }}
    };

    ui::HeadlessRenderer renderer{{80.0f, 90.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red(renderer.pixel(25, 25)));
    NUI_CHECK(!red(renderer.pixel(72, 25)));
    NUI_CHECK(green(renderer.pixel(30, 62)));

    // Empty and degenerate paths are valid no-ops and must never crash.
    ui::Path empty;
    ui::Path degenerate;
    degenerate.move_to({20.0f, 20.0f})
              .line_to({20.0f, 20.0f})
              .quad_to({20.0f, 20.0f}, {20.0f, 20.0f})
              .cubic_to({20.0f, 20.0f}, {20.0f, 20.0f}, {20.0f, 20.0f})
              .close();

    ui::UI degenerate_tree{
        ui::Canvas{40.0f, 40.0f, [empty, degenerate](ui::CanvasContext2D& g) {
            g.fill_path(empty, {1.0f, 0.0f, 0.0f, 1.0f});
            g.stroke_path(empty, {1.0f, 0.0f, 0.0f, 1.0f}, {});
            g.fill_path(degenerate, {1.0f, 0.0f, 0.0f, 1.0f});
            g.stroke_path(degenerate, {1.0f, 0.0f, 0.0f, 1.0f}, {});
        }}
    };

    ui::HeadlessRenderer degenerate_renderer{{40.0f, 40.0f}, 1.0f};
    NUI_CHECK(degenerate_renderer.render(degenerate_tree));
    NUI_CHECK(!red(degenerate_renderer.pixel(5, 5)));
}

} // namespace

int main() { return test::run("paths", suite); }
