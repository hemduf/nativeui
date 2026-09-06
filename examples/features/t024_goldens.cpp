#include "example_support.hpp"

namespace {

std::unique_ptr<ui::UI> make_ui(ui::State<bool>& enabled) {
    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T024 / GOLDEN RENDERING"},
            ui::Row{
                ui::Canvas{64.0f, 64.0f, [](ui::CanvasContext2D& g) {
                    g.fill_rect({0.0f, 0.0f, 64.0f, 64.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                }},
                ui::Canvas{64.0f, 64.0f, [](ui::CanvasContext2D& g) {
                    g.fill_rect({0.0f, 0.0f, 32.0f, 64.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                    g.fill_rect({32.0f, 0.0f, 32.0f, 64.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
                }}
            }.gap(12.0f),
            ui::Toggle{"Golden geometry", enabled},
            ui::Canvas{420.0f, 48.0f, [](ui::CanvasContext2D& g) {
                g.text({0.0f, 18.0f},
                       "CTest compares versioned PPM baselines; updates are explicit.",
                       11.0f,
                       ui::colors::textMuted);
            }}
        }.padding(20.0f).gap(14.0f));
}

} // namespace

int main(int argc, char** argv) {
    ui::State<bool> enabled{true};
    auto tree = make_ui(enabled);

    if (example::self_test_requested(argc, argv)) {
        ui::HeadlessRenderer renderer{{520.0f, 330.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        const auto red = renderer.pixel(24, 108);
        if (!(red.r > 220 && red.g < 40 && red.b < 40)) {
            return example::fail("Canvas golden scene did not render red geometry");
        }
        enabled.set(false);
        if (!tree->paint_dirty()) return example::fail("Toggle state did not invalidate paint");
        if (!renderer.render(*tree)) return example::fail("second headless render failed");
        return 0;
    }

    return example::run_window(*tree, "NativeUI T024 - Golden Rendering", {560.0f, 380.0f});
}
