#include "example_support.hpp"

namespace {
bool is_red(ui::Rgba8 p) { return p.r > 200 && p.g < 80 && p.b < 80; }
}

int main(int argc, char** argv) {
    auto make_ui = [] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T011 / CLIPPING"},
                ui::Row{
                    ui::Clip{
                        ui::Canvas{80.0f, 90.0f, [](ui::CanvasContext2D& g) {
                            g.fill_rect({-30.0f, 12.0f, 160.0f, 56.0f}, {1.0f, 0.08f, 0.08f, 1.0f});
                            g.text({40.0f, 40.0f}, "CLIPPED", 11.0f, ui::colors::text, ui::TextAlign::Center);
                        }}},
                    ui::Spacer{100.0f, 90.0f}}
                    .gap(0.0f),
                ui::Canvas{240.0f, 50.0f, [](ui::CanvasContext2D& g) {
                    g.text({0.0f, 18.0f}, "The red bar is intentionally wider than its 80px viewport.", 11.0f, ui::colors::textMuted);
                }}
            }.padding(18.0f).gap(12.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        ui::UI probe{
            ui::Row{
                ui::Clip{ui::Canvas{40.0f, 40.0f, [](ui::CanvasContext2D& g) {
                    g.fill_rect({-10.0f, 0.0f, 80.0f, 40.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                }}},
                ui::Spacer{60.0f, 40.0f}}
                .gap(0.0f)};
        ui::HeadlessRenderer renderer{{100.0f, 60.0f}};
        if (!renderer.render(probe)) return example::fail("headless render failed");
        if (!is_red(renderer.pixel(20, 10))) return example::fail("clip content missing inside viewport");
        if (is_red(renderer.pixel(60, 10))) return example::fail("overflow escaped clip viewport");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T011 - Clipping", {420.0f, 280.0f});
}
