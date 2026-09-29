#include "example_support.hpp"

namespace {
void draw_scene(ui::CanvasContext2D& g) {
    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);

    g.save();
    g.translate(130.0f, 90.0f);
    g.rotate(ui::kPi * 0.12f);
    g.scale(1.25f, 0.85f);
    g.fill_rounded_rect({-45.0f, -30.0f, 90.0f, 60.0f}, 8.0f, ui::colors::accent);
    g.restore();

    g.save();
    g.concat(ui::Transform2D::translation(310.0f, 60.0f));
    g.concat(ui::Transform2D::scaling(1.4f, 1.4f));
    g.circle({0.0f, 0.0f}, 24.0f, ui::colors::caret);
    g.restore();

    g.text({16.0f, 170.0f}, "save/restore + translate + scale + rotate + concat", 11.0f, ui::colors::textMuted);
}
}

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T019 / 2D TRANSFORMS"},
                ui::Canvas{520.0f, 190.0f, draw_scene}
            }.padding(20.0f).gap(12.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{580.0f, 310.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        const auto accent = renderer.pixel(150, 185);
        if (accent.a == 0) return example::fail("transformed content was not rendered");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T019 - Transforms", {600.0f, 360.0f});
}
