#include "test_support.hpp"

namespace {

bool red(ui::Rgba8 p) {
    return p.r > 220 && p.g < 50 && p.b < 50 && p.a > 220;
}

bool green(ui::Rgba8 p) {
    return p.g > 220 && p.r < 50 && p.b < 50 && p.a > 220;
}

bool painted_green(ui::Rgba8 p) {
    return p.g > 100 && p.g > p.r * 2 && p.g > p.b * 2;
}

bool dark(ui::Rgba8 p) {
    return p.r < 20 && p.g < 20 && p.b < 20;
}

void brush_stroke_style_regressions() {
    const ui::Brush brush{ui::Color{0.0f, 1.0f, 0.0f, 1.0f}};

    ui::Path butt;
    butt.move_to({10.0f, 10.0f}).line_to({20.0f, 10.0f});
    ui::Path round_cap;
    round_cap.move_to({40.0f, 10.0f}).line_to({50.0f, 10.0f});
    ui::Path square_cap;
    square_cap.move_to({70.0f, 10.0f}).line_to({80.0f, 10.0f});

    auto join_path = [](float center_x) {
        ui::Path path;
        path.move_to({center_x - 10.0f, 60.0f})
            .line_to({center_x, 50.0f})
            .line_to({center_x + 10.0f, 60.0f});
        return path;
    };
    const auto miter = join_path(20.0f);
    const auto round_join = join_path(50.0f);
    const auto bevel = join_path(80.0f);
    const auto limited_miter = join_path(110.0f);

    ui::UI tree{
        ui::Canvas{140.0f, 72.0f,
            [brush, butt, round_cap, square_cap, miter, round_join, bevel, limited_miter]
            (ui::CanvasContext2D& g) {
                g.stroke_path(butt, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Butt,
                                              ui::StrokeJoin::Miter, 4.0f});
                g.stroke_path(round_cap, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Round,
                                              ui::StrokeJoin::Miter, 4.0f});
                g.stroke_path(square_cap, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Square,
                                              ui::StrokeJoin::Miter, 4.0f});
                g.stroke_path(miter, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Butt,
                                              ui::StrokeJoin::Miter, 4.0f});
                g.stroke_path(round_join, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Butt,
                                              ui::StrokeJoin::Round, 4.0f});
                g.stroke_path(bevel, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Butt,
                                              ui::StrokeJoin::Bevel, 4.0f});
                g.stroke_path(limited_miter, brush,
                              ui::StrokeStyle{10.0f, ui::StrokeCap::Butt,
                                              ui::StrokeJoin::Miter, 1.0f});
            }}
    };

    ui::HeadlessRenderer renderer{{140.0f, 72.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    // Butt stops at the endpoint; Round extends radially; Square extends the
    // full stroke-width rectangle into the corner outside the round cap.
    NUI_CHECK(dark(renderer.pixel(24, 10)));
    NUI_CHECK(painted_green(renderer.pixel(54, 10)));
    NUI_CHECK(dark(renderer.pixel(54, 14)));
    NUI_CHECK(painted_green(renderer.pixel(84, 14)));

    // At this 90-degree V, a normal miter reaches above both Round and Bevel.
    // A miter limit of 1 deterministically falls back before that tip.
    NUI_CHECK(painted_green(renderer.pixel(20, 43)));
    NUI_CHECK(dark(renderer.pixel(50, 43)));
    NUI_CHECK(dark(renderer.pixel(80, 43)));
    NUI_CHECK(dark(renderer.pixel(110, 43)));

    // Round occupies the curved shoulder that Bevel cuts away.
    NUI_CHECK(painted_green(renderer.pixel(50, 45)));
    NUI_CHECK(dark(renderer.pixel(80, 45)));
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
    // Sample the center of the first stroked segment, well away from the join.
    NUI_CHECK(green(renderer.pixel(25, 61)));

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

    brush_stroke_style_regressions();
}

} // namespace

int main() { return test::run("paths", suite); }
