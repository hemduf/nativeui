#include "example_support.hpp"
#include <nativeui/grid.hpp>
#include <stdexcept>

namespace {

int self_test() {
    auto automatic = std::make_shared<example::BoxObservation>();
    auto spanning = std::make_shared<example::BoxObservation>();
    ui::UI tree{ui::Grid{ui::GridTracks{
        .columns={ui::Track::fixed(40.0f), ui::Track::auto_size(), ui::Track::flex()},
        .rows={ui::Track::auto_size()}},
        example::Box{"Auto", {20.0f,30.0f}, ui::colors::accent, {}, automatic}}
        .cell({0,1,1,2}, example::Box{"Span", {110.0f,40.0f}, ui::colors::accent,
                                     {20.0f,10.0f}, spanning}).gap(5.0f)};
    if (!example::near(tree.measure().preferred.w, 155.0f))
        return example::fail("span contribution altered the fixed track");
    tree.resize({300.0f,80.0f});
    ui::HeadlessRenderer renderer{{300.0f,80.0f},1.0f};
    if (!renderer.render(tree)) return example::fail("headless Grid render failed");
    if (!example::near(automatic->bounds.x, 0.0f) ||
        !example::near(automatic->bounds.w, 40.0f) ||
        !example::near(spanning->bounds.x, 45.0f) ||
        !example::near(spanning->bounds.w, 255.0f))
        return example::fail("explicit span and automatic placement disagreed");
    bool rejected = false;
    try {
        (void)ui::Grid{ui::GridTracks{}}
            .cell({0,0,1,1}, ui::Spacer{10.0f,10.0f})
            .cell({0,0,1,1}, ui::Spacer{10.0f,10.0f}).spec();
    } catch (const std::invalid_argument&) { rejected = true; }
    return rejected ? 0 : example::fail("overlapping explicit cells were published");
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::UI tree{ui::Column{
        ui::Header{"Grid cells and spans"},
        ui::Grid{ui::GridTracks{
            .columns={ui::Track::fixed(120.0f), ui::Track::flex(), ui::Track::flex()},
            .rows={ui::Track::auto_size(), ui::Track::auto_size()}},
            example::Box{"Automatic first free cell", {120.0f,50.0f}},
            example::Box{"Automatic next row", {120.0f,50.0f}}}
            .cell({0,1,1,2}, example::Box{"Two flexible columns", {240.0f,50.0f}})
            .cell({1,1,1,2}, ui::Label{"Explicit cells reserve space before automatic children"})
            .gap(8.0f)}.padding(20.0f)};
    return example::run_window(tree, "NativeUI Grid", {620.0f,240.0f});
}
