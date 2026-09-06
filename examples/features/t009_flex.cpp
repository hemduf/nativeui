#include "example_support.hpp"

int main(int argc, char** argv) {
    auto one = std::make_shared<example::BoxObservation>();
    auto two = std::make_shared<example::BoxObservation>();

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Row{
                ui::Flex{example::Box{"grow 1", {80.0f, 80.0f}, ui::colors::accent, {50.0f, 50.0f}, one}}
                    .grow(1.0f).shrink(1.0f),
                ui::Flex{example::Box{"grow 2", {80.0f, 80.0f}, ui::colors::toggleOff, {50.0f, 50.0f}, two}}
                    .grow(2.0f).shrink(1.0f)}
                .gap(10.0f)
                .align(ui::Align::Stretch));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{400.0f, 100.0f}};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        if (!(two->bounds.w > one->bounds.w)) return example::fail("grow weight 2 did not receive more space");
        if (one->bounds.w < 50.0f || two->bounds.w < 50.0f) return example::fail("minimum flex size violated");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T009 - Flex", {620.0f, 220.0f});
}
