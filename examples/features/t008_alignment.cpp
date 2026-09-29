#include "example_support.hpp"

int main(int argc, char** argv) {
    auto small = std::make_shared<example::BoxObservation>();
    auto tall = std::make_shared<example::BoxObservation>();

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Row{
                example::Box{"40 high", {100.0f, 40.0f}, ui::colors::accent, {}, small},
                example::Box{"100 high", {100.0f, 100.0f}, ui::colors::toggleOff, {}, tall}}
                .gap(16.0f)
                .align(ui::Align::Center)
                .justify(ui::Justify::SpaceBetween));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{320.0f, 140.0f}};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        if (!(small->bounds.y > tall->bounds.y)) return example::fail("center alignment not visible");
        if (!example::near(small->bounds.y + small->bounds.h * 0.5f,
                           tall->bounds.y + tall->bounds.h * 0.5f)) {
            return example::fail("cross-axis centers do not match");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T008 - Alignment", {520.0f, 220.0f});
}
