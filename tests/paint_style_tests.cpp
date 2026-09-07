#include "test_support.hpp"

namespace {

void two_stop_linear_gradient() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    };

    ui::UI tree{
        ui::Canvas{32.0f, 8.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 8.0f}, gradient);
        }}
    };

    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(2, 4);
    const auto middle = renderer.pixel(16, 4);
    const auto right = renderer.pixel(29, 4);

    NUI_CHECK(left.r > 180 && left.b < 80);
    NUI_CHECK(middle.r > 70 && middle.b > 70);
    NUI_CHECK(right.b > 180 && right.r < 80);
}

void multi_stop_linear_gradient() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            ui::GradientStop{0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
    };

    ui::UI tree{
        ui::Canvas{32.0f, 8.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 8.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(2, 4);
    const auto middle = renderer.pixel(16, 4);
    const auto right = renderer.pixel(29, 4);
    NUI_CHECK(left.r > left.g && left.r > left.b);
    NUI_CHECK(middle.g > middle.r && middle.g > middle.b);
    NUI_CHECK(right.b > right.r && right.b > right.g);
}

void radial_gradient() {
    const ui::RadialGradient gradient{
        {16.0f, 8.0f},
        8.0f,
        {
            ui::GradientStop{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
        },
    };

    ui::UI tree{
        ui::Canvas{32.0f, 16.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto center = renderer.pixel(16, 8);
    const auto edge = renderer.pixel(23, 8);
    NUI_CHECK(center.r > 220 && center.g > 220 && center.b > 220);
    NUI_CHECK(edge.r < 80 && edge.g < 80 && edge.b < 80);
}

void suite() {
    two_stop_linear_gradient();
    multi_stop_linear_gradient();
    radial_gradient();
}

} // namespace

int main() { return test::run("paint styles", suite); }
