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

void theme_change_classification() {
    const auto defaults = ui::default_theme();
    NUI_CHECK(ui::classify_theme_change(defaults, defaults) == ui::ThemeInvalidation::None);

    auto paint = defaults;
    paint.palette.accent = ui::Color{0.1f, 0.2f, 0.3f, 1.0f};
    NUI_CHECK(ui::classify_theme_change(defaults, paint) == ui::ThemeInvalidation::Paint);

    auto radius = defaults;
    radius.radii.medium += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, radius) == ui::ThemeInvalidation::Paint);

    auto border = defaults;
    border.controls.border_width += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, border) == ui::ThemeInvalidation::Paint);

    auto thumb = defaults;
    thumb.controls.thumb_diameter += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, thumb) == ui::ThemeInvalidation::Paint);

    auto typography = defaults;
    typography.typography.control_size += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, typography) == ui::ThemeInvalidation::Layout);

    auto spacing = defaults;
    spacing.spacing.medium += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, spacing) == ui::ThemeInvalidation::Layout);

    auto controls = defaults;
    controls.controls.control_height += 1.0f;
    NUI_CHECK(ui::classify_theme_change(defaults, controls) == ui::ThemeInvalidation::Layout);
}

void ui_theme_ownership_and_invalidation() {
    test::MockPlatform platform;
    SkCanvas canvas;

    auto initial = ui::default_theme();
    initial.palette.background = ui::Color{0.12f, 0.13f, 0.14f, 1.0f};
    ui::UI tree{ui::Button{"Theme", [] {}}, initial};
    tree.resize({180.0f, 64.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());
    NUI_CHECK(tree.theme().palette.background.r == initial.palette.background.r);

    const auto same = tree.theme();
    tree.set_theme(same);
    NUI_CHECK(!tree.dirty());

    auto paint_only = tree.theme();
    paint_only.palette.accent = ui::Color{0.2f, 0.3f, 0.4f, 1.0f};
    tree.set_theme(paint_only);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());

    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    auto layout = tree.theme();
    layout.typography.control_size += 3.0f;
    tree.set_theme(layout);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(tree.layout_dirty());

    auto other_theme = ui::default_theme();
    other_theme.palette.background = ui::Color{0.41f, 0.42f, 0.43f, 1.0f};
    ui::UI other{ui::Button{"Other", [] {}}, other_theme};
    NUI_CHECK(tree.theme().palette.background.r != other.theme().palette.background.r);
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

    theme_change_classification();
    ui_theme_ownership_and_invalidation();
}

} // namespace

int main() { return test::run("invalidation", &suite); }
