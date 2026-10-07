#include "example_support.hpp"
#include <nativeui/padding.hpp>

#include <limits>

namespace {

int self_test() {
    ui::PaddingComponent padding{std::numeric_limits<float>::infinity()};
    std::vector<ui::ChildMetrics> children{ui::ChildMetrics{{40.0f, 20.0f}}};
    if (!example::near(padding.measure(children).w, 40.0f))
        return example::fail("non-finite padding changed intrinsic measurement");
    std::vector<ui::ChildPlacement> placements(1);
    padding.layout_children({3.0f, 4.0f, 90.0f, 60.0f}, children, placements);
    if (!example::near(placements.front().bounds.x, 3.0f) ||
        !example::near(placements.front().bounds.w, 90.0f))
        return example::fail("non-finite padding escaped into placement");
    ui::UI tree{ui::Padding{12.0f, ui::Label{"Uniform logical padding"}}};
    tree.resize({240.0f, 60.0f});
    ui::HeadlessRenderer renderer{{240.0f, 60.0f}, 1.0f};
    return renderer.render(tree) ? 0 : example::fail("headless Padding render failed");
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::UI tree{ui::Column{
        ui::Header{"Padding"},
        ui::Padding{24.0f, ui::Label{"Uniform inset on both logical axes"}},
        ui::Padding{std::numeric_limits<float>::infinity(),
                    ui::Label{"Non-finite padding is normalized to zero"}}}.padding(20.0f)};
    return example::run_window(tree, "NativeUI Padding", {480.0f, 220.0f});
}
