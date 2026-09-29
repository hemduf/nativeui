#include "example_support.hpp"

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        ui::UI probe{example::Box{"probe", {200.0f, 80.0f}, ui::colors::accent, {60.0f, 30.0f}}};
        const auto metrics = probe.measure(ui::Constraints::tight({120.0f, 50.0f}));
        if (!example::near(metrics.preferred.w, 120.0f) ||
            !example::near(metrics.preferred.h, 50.0f)) {
            return example::fail("tight constraints were not honored");
        }
        if (metrics.minimum.w > metrics.preferred.w || metrics.minimum.h > metrics.preferred.h) {
            return example::fail("minimum exceeded preferred size");
        }
        return 0;
    }

    ui::UI app{
        ui::Column{
            ui::Header{"T007 / LAYOUT CONSTRAINTS"},
            ui::Row{
                example::Box{"min 80 / pref 220", {220.0f, 90.0f}, ui::colors::accent, {80.0f, 50.0f}},
                example::Box{"min 60 / pref 140", {140.0f, 60.0f}, ui::colors::toggleOff, {60.0f, 40.0f}},
            }.gap(12.0f).align(ui::Align::Center),
            ui::Canvas{620.0f, 70.0f, [](ui::CanvasContext2D& g) {
                g.text({12.0f, 24.0f}, "Resize the window: children stay finite and respect their minimum/preferred sizes.",
                       12.0f, ui::colors::textMuted);
            }}
        }.padding(20.0f).gap(16.0f)};
    return example::run_window(app, "NativeUI T007 - Constraints", {680.0f, 300.0f});
}
