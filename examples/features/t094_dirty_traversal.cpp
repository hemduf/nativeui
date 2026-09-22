#include "../../tests/test_support.hpp"

#include "include/core/SkCanvas.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

struct PaintProbeState {
    int paints{};
};

class PaintProbeComponent final : public ui::Component {
public:
    explicit PaintProbeComponent(std::shared_ptr<PaintProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {40.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override { ++state_->paints; }

private:
    std::shared_ptr<PaintProbeState> state_;
};

class SplitRootComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200.0f, 100.0f};
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        placements.at(0).bounds = {bounds.x, bounds.y, 60.0f, bounds.h};
        placements.at(1).bounds = {bounds.x + 140.0f, bounds.y, 60.0f, bounds.h};
    }

    void paint(ui::PaintContext&) const override {}
};

ui::Spec probe(std::shared_ptr<PaintProbeState> state) {
    return ui::Spec{
        [state = std::move(state)] {
            return std::make_unique<PaintProbeComponent>(state);
        },
        {}};
}

void red_dirty_traversal_contract() {
    auto left = std::make_shared<PaintProbeState>();
    auto right = std::make_shared<PaintProbeState>();

    ui::Spec root{
        [] { return std::make_unique<SplitRootComponent>(); },
        {probe(left), probe(right)}};

    ui::Tree tree{ui::compile(std::move(root))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({200.0f, 100.0f});
    tree.paint(canvas, platform);

    left->paints = 0;
    right->paints = 0;

    // RED before T094: there is no retained traversal entry point capable of
    // restricting callbacks to a canonical root-logical repaint region.
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 80.0f, 100.0f});

    NUI_CHECK(left->paints == 1);
    NUI_CHECK(right->paints == 0);
}

} // namespace

int main() {
    return test::run("T094 dirty retained traversal RED", red_dirty_traversal_contract);
}
