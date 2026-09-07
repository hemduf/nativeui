#include "test_support.hpp"

namespace {

bool red(ui::Rgba8 p) { return p.r > 220 && p.g < 50 && p.b < 50 && p.a > 220; }
bool green(ui::Rgba8 p) { return p.g > 220 && p.r < 50 && p.b < 50 && p.a > 220; }
bool blue(ui::Rgba8 p) { return p.b > 220 && p.r < 50 && p.g < 50 && p.a > 220; }

void suite() {
    // Nested save/restore restores the previous transform exactly.
    {
        ui::UI tree{
            ui::Canvas{80.0f, 50.0f, [](ui::CanvasContext2D& g) {
                g.save();
                g.translate(20.0f, 0.0f);
                g.fill_rect({0.0f, 0.0f, 10.0f, 10.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                g.save();
                g.translate(20.0f, 0.0f);
                g.fill_rect({0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 0.0f, 1.0f, 1.0f});
                g.restore();
                g.restore();
                g.fill_rect({0.0f, 20.0f, 10.0f, 10.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
            }}
        };
        ui::HeadlessRenderer renderer{{80.0f, 50.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(25, 5)));
        NUI_CHECK(blue(renderer.pixel(45, 5)));
        NUI_CHECK(green(renderer.pixel(5, 25)));
        NUI_CHECK(!green(renderer.pixel(25, 25)));
    }

    // A component-level transform is isolated automatically even without a
    // matching user save/restore, so siblings start from a clean canvas state.
    {
        ui::UI tree{
            ui::Row{
                ui::Canvas{40.0f, 40.0f, [](ui::CanvasContext2D& g) {
                    g.translate(10.0f, 0.0f);
                    g.fill_rect({0.0f, 0.0f, 10.0f, 10.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                }},
                ui::Canvas{40.0f, 40.0f, [](ui::CanvasContext2D& g) {
                    g.fill_rect({0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
                }}
            }.gap(0.0f)
        };
        ui::HeadlessRenderer renderer{{80.0f, 40.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(15, 5)));
        NUI_CHECK(green(renderer.pixel(45, 5)));
        NUI_CHECK(!green(renderer.pixel(55, 5)));
    }

    // Canvas transforms are local even when layout places the Canvas away
    // from the window origin. Scaling must preserve the Canvas origin rather
    // than scaling its absolute layout position.
    {
        ui::UI tree{
            ui::Padding{20.0f,
                ui::Canvas{30.0f, 30.0f, [](ui::CanvasContext2D& g) {
                    g.scale(2.0f);
                    g.fill_rect({0.0f, 0.0f, 5.0f, 5.0f},
                                {1.0f, 0.0f, 0.0f, 1.0f});
                }}}
        };
        ui::HeadlessRenderer renderer{{70.0f, 70.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(25, 25)));
        NUI_CHECK(!red(renderer.pixel(45, 45)));
    }

    // concat() uses the same affine convention as translate().
    {
        ui::UI tree{
            ui::Canvas{50.0f, 30.0f, [](ui::CanvasContext2D& g) {
                g.concat(ui::Transform2D::translation(15.0f, 0.0f));
                g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
            }}
        };
        ui::HeadlessRenderer renderer{{50.0f, 30.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(18, 4)));
        NUI_CHECK(!red(renderer.pixel(4, 4)));
    }

    // T020 red unit: arbitrary paths remain local to Canvas transforms and do
    // not expose backend-specific geometry to the caller.
    {
        ui::Path path;
        path.move_to({0.0f, 0.0f})
            .line_to({12.0f, 0.0f})
            .line_to({12.0f, 12.0f})
            .close();

        ui::UI tree{
            ui::Padding{10.0f,
                ui::Canvas{30.0f, 30.0f, [path](ui::CanvasContext2D& g) {
                    g.translate(5.0f, 5.0f);
                    g.fill_path(path, {1.0f, 0.0f, 0.0f, 1.0f});
                }}}
        };
        ui::HeadlessRenderer renderer{{50.0f, 50.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(20, 18)));
        NUI_CHECK(!red(renderer.pixel(8, 8)));
    }
}

} // namespace

int main() { return test::run("transforms", suite); }
