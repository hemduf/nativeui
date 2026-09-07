#include "test_support.hpp"

namespace {

void suite() {
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

} // namespace

int main() { return test::run("paint styles", suite); }
