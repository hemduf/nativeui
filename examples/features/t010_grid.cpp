#include "example_support.hpp"

int main(int argc, char** argv) {
    auto a = std::make_shared<example::BoxObservation>();
    auto b = std::make_shared<example::BoxObservation>();
    auto c = std::make_shared<example::BoxObservation>();
    auto d = std::make_shared<example::BoxObservation>();

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Grid{
                ui::GridTracks{
                    {ui::Track::fixed(100.0f), ui::Track::flex(1.0f)},
                    {ui::Track::flex(1.0f), ui::Track::flex(1.0f)}},
                example::Box{"fixed", {80.0f, 60.0f}, ui::colors::accent, {}, a},
                example::Box{"flex", {80.0f, 60.0f}, ui::colors::toggleOff, {}, b},
                example::Box{"row 2", {80.0f, 60.0f}, ui::colors::toggleOff, {}, c},
                example::Box{"row 2", {80.0f, 60.0f}, ui::colors::accent, {}, d}}
                .gap(10.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{360.0f, 220.0f}};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        if (!example::near(a->bounds.w, 100.0f)) return example::fail("fixed grid track changed size");
        if (!(b->bounds.w > a->bounds.w)) return example::fail("flex grid track did not consume remaining space");
        if (!(c->bounds.y > a->bounds.y)) return example::fail("second grid row not placed below first");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T010 - Grid", {620.0f, 360.0f});
}
