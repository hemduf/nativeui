#include "test_support.hpp"

#include "include/core/SkCanvas.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace {

struct InvalidationProbeState {
    int measures{};
    std::function<void()> invalidate_paint;
    std::function<void()> invalidate_layout;
};

class InvalidationProbeComponent final : public ui::Component {
public:
    explicit InvalidationProbeComponent(std::shared_ptr<InvalidationProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        ++state_->measures;
        return {100.0f, 40.0f};
    }

    void mount(ui::MountContext& context) override {
        state_->invalidate_paint = context.invalidator();
        state_->invalidate_layout = context.layout_invalidator();
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<InvalidationProbeState> state_;
};

class InvalidationProbe {
public:
    explicit InvalidationProbe(std::shared_ptr<InvalidationProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<InvalidationProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<InvalidationProbeState> state_;
};

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.0001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.0001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

void suite() {
    // Dirty rectangles are clipped, merged, and duplicate coverage is ignored.
    {
        ui::DirtyRegion dirty;
        const ui::Rect clip{0.0f, 0.0f, 100.0f, 80.0f};
        auto first = dirty.add({-10.0f, -10.0f, 20.0f, 20.0f}, clip);
        NUI_CHECK(first.has_value());
        check_rect(*first, {0.0f, 0.0f, 10.0f, 10.0f});

        auto merged = dirty.add({10.0f, 0.0f, 10.0f, 10.0f}, clip);
        NUI_CHECK(merged.has_value());
        check_rect(*merged, {0.0f, 0.0f, 20.0f, 10.0f});
        NUI_CHECK(dirty.rects().size() == 1);

        NUI_CHECK(!dirty.add({2.0f, 2.0f, 4.0f, 4.0f}, clip).has_value());
        NUI_CHECK(!dirty.add({120.0f, 0.0f, 5.0f, 5.0f}, clip).has_value());
    }

    // Component paint invalidation is local and never forces a layout pass.
    {
        auto probe = std::make_shared<InvalidationProbeState>();
        ui::UI tree{ui::Padding{10.0f, InvalidationProbe{probe}}};
        test::MockPlatform platform;
        SkCanvas canvas;

        tree.resize({200.0f, 100.0f});
        tree.paint(canvas, platform);
        NUI_CHECK(!tree.dirty());
        NUI_CHECK(!tree.layout_dirty());
        const int measures_after_layout = probe->measures;

        std::vector<ui::Rect> exposures;
        tree.set_invalidation_callback([&](ui::Rect rect) { exposures.push_back(rect); });
        NUI_CHECK(exposures.empty());

        probe->invalidate_paint();
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(exposures.size() == 1);
        check_rect(exposures.back(), {10.0f, 10.0f, 180.0f, 80.0f});
        NUI_CHECK(probe->measures == measures_after_layout);

        // A duplicate paint invalidation does not schedule another exposure.
        probe->invalidate_paint();
        NUI_CHECK(exposures.size() == 1);

        tree.paint(canvas, platform);
        NUI_CHECK(!tree.dirty());
        NUI_CHECK(probe->measures == measures_after_layout);

        // Layout invalidation propagates to ancestors, schedules a safe full
        // repaint, and re-runs layout lazily immediately before paint.
        exposures.clear();
        probe->invalidate_layout();
        NUI_CHECK(tree.layout_dirty());
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(exposures.size() == 1);
        check_rect(exposures.back(), {0.0f, 0.0f, 200.0f, 100.0f});
        NUI_CHECK(probe->measures == measures_after_layout);

        tree.paint(canvas, platform);
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(!tree.paint_dirty());
        NUI_CHECK(probe->measures > measures_after_layout);

        // Painting an already-clean tree never schedules a redraw by itself.
        exposures.clear();
        tree.paint(canvas, platform);
        NUI_CHECK(exposures.empty());
    }

    // Existing state-bound widgets use paint-only invalidation by default.
    {
        ui::State<float> value{0.25f};
        ui::UI tree{ui::Knob{"Value", value}};
        test::MockPlatform platform;
        SkCanvas canvas;
        tree.resize({180.0f, 190.0f});
        tree.paint(canvas, platform);
        NUI_CHECK(!tree.dirty());

        value.set(0.75f);
        NUI_CHECK(tree.paint_dirty());
        NUI_CHECK(!tree.layout_dirty());
    }
}

} // namespace

int main() { return test::run("invalidation", &suite); }
