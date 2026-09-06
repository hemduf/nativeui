#include "test_support.hpp"

namespace {

bool accent(ui::Rgba8 p) {
    return p.r > 220 && p.g > 120 && p.g < 190 && p.b < 100 && p.a > 220;
}

void suite() {
    // Vertical scrolling exposes metrics and repositions content by a clamped offset.
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{
            ui::Scroll{state,
                ui::Canvas{ui::Size{80.0f, 240.0f}, [](ui::CanvasContext2D& g) {
                    g.fill_rect({0.0f, 0.0f, 80.0f, 20.0f}, ui::colors::accent);
                    g.fill_rect({0.0f, 100.0f, 80.0f, 20.0f}, ui::colors::accent);
                }}}};

        ui::HeadlessRenderer renderer{{80.0f, 80.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.viewport_size().w, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().h, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().w, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().h, 240.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));

        state.set_offset({0.0f, 100.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 0.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));

        state.set_offset({50.0f, 1000.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 0.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 160.0f, 0.001f);

        state.scroll_by({0.0f, -1000.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);

        int observed = 0;
        auto subscription = state.observe([&](ui::Point) { ++observed; });
        state.set_offset({0.0f, 150.0f});
        NUI_CHECK(renderer.render(tree));
        renderer.resize({80.0f, 200.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().y, 40.0f, 0.001f);
        NUI_CHECK(observed >= 2); // explicit set + automatic clamp after viewport resize
    }

    // Horizontal scrolling keeps the cross axis constrained to the viewport.
    {
        ui::ScrollState state{ui::ScrollAxis::Horizontal};
        ui::UI tree{
            ui::Scroll{state,
                ui::Canvas{ui::Size{220.0f, 30.0f}, [](ui::CanvasContext2D& g) {
                    g.fill_rect({100.0f, 0.0f, 20.0f, 30.0f}, ui::colors::accent);
                }}}};

        ui::HeadlessRenderer renderer{{70.0f, 50.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.content_size().w, 220.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().w, 70.0f, 0.001f);
        state.set_offset({100.0f, 30.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 100.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));
    }

    // Both-axis mode measures nested natural content while the viewport remains fixed.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        ui::UI tree{
            ui::Scroll{state,
                ui::Column{
                    ui::Spacer{180.0f, 40.0f},
                    ui::Spacer{180.0f, 50.0f},
                    ui::Spacer{180.0f, 60.0f}}
                    .gap(5.0f)
                    .padding(0.0f)}};

        ui::HeadlessRenderer renderer{{90.0f, 70.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.viewport_size().w, 90.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().h, 70.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().w, 180.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().h, 160.0f, 0.001f);
        state.set_offset({999.0f, 999.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 90.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 90.0f, 0.001f);
    }
}

} // namespace

int main() { return test::run("scroll_layout", &suite); }
