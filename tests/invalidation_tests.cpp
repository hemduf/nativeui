#include "test_support.hpp"

#include "include/core/SkCanvas.h"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/virtual_list_retained.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace {

struct InvalidationProbeState {
    int measures{};
    ui::NodeId node_id{ui::kInvalidNodeId};
    std::function<void()> invalidate_paint;
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_focus;
    std::function<void()> invalidate_availability;
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
        state_->node_id = context.node_id();
        state_->invalidate_paint = context.invalidator();
        state_->invalidate_layout = context.layout_invalidator();
        state_->invalidate_focus = context.focus_invalidator();
        state_->invalidate_availability = context.availability_invalidator();
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

class SpecRoot {
public:
    explicit SpecRoot(ui::Spec value) : value_(std::move(value)) {}
    [[nodiscard]] ui::Spec spec() && { return std::move(value_); }

private:
    ui::Spec value_;
};

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.0001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.0001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

void invoke_retained_invalidators(const InvalidationProbeState& probe) {
    probe.invalidate_paint();
    probe.invalidate_layout();
    probe.invalidate_focus();
    probe.invalidate_availability();
}

void retained_invalidator_stale_node_contract() {
    auto probe = std::make_shared<InvalidationProbeState>();
    ui::State<bool> visible{true};
    test::MockPlatform platform;
    SkCanvas canvas;

    ui::UI tree{ui::If{visible, InvalidationProbe{probe}}};
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    auto previous_id = probe->node_id;
    std::vector<ui::NodeId> observed_ids{previous_id};
    const auto first_stale_paint = probe->invalidate_paint;
    const auto first_stale_layout = probe->invalidate_layout;
    const auto first_stale_focus = probe->invalidate_focus;
    const auto first_stale_availability = probe->invalidate_availability;

    for (int cycle = 0; cycle < 4; ++cycle) {
        const auto stale_paint = probe->invalidate_paint;
        const auto stale_layout = probe->invalidate_layout;
        const auto stale_focus = probe->invalidate_focus;
        const auto stale_availability = probe->invalidate_availability;

        visible.set(false);
        tree.resize({200.0f, 100.0f});
        tree.paint(canvas, platform);
        NUI_CHECK(!tree.dirty());

        stale_paint();
        stale_layout();
        stale_focus();
        stale_availability();
        NUI_CHECK(!tree.dirty());

        visible.set(true);
        tree.resize({200.0f, 100.0f});
        tree.paint(canvas, platform);
        NUI_CHECK(probe->node_id != ui::kInvalidNodeId);
        NUI_CHECK(probe->node_id > previous_id);
        NUI_CHECK(std::find(observed_ids.begin(), observed_ids.end(), probe->node_id) ==
                  observed_ids.end());
        observed_ids.push_back(probe->node_id);
        previous_id = probe->node_id;
        NUI_CHECK(!tree.dirty());

        stale_paint();
        stale_layout();
        stale_focus();
        stale_availability();
        NUI_CHECK(!tree.dirty());

        first_stale_paint();
        first_stale_layout();
        first_stale_focus();
        first_stale_availability();
        NUI_CHECK(!tree.dirty());
    }
}

void retained_invalidator_tree_lifetime_contract() {
    std::function<void()> stale_paint;
    std::function<void()> stale_layout;
    std::function<void()> stale_focus;
    std::function<void()> stale_availability;

    {
        auto probe = std::make_shared<InvalidationProbeState>();
        ui::UI tree{InvalidationProbe{probe}};
        test::MockPlatform platform;
        SkCanvas canvas;
        tree.resize({160.0f, 80.0f});
        tree.paint(canvas, platform);

        stale_paint = probe->invalidate_paint;
        stale_layout = probe->invalidate_layout;
        stale_focus = probe->invalidate_focus;
        stale_availability = probe->invalidate_availability;
    }

    stale_paint();
    stale_layout();
    stale_focus();
    stale_availability();
}

void retained_animation_target_removal_contract() {
    auto probe = std::make_shared<InvalidationProbeState>();
    ui::State<bool> visible{true};
    test::MockPlatform platform;
    SkCanvas canvas;

    ui::UI tree{ui::If{visible, InvalidationProbe{probe}}};
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{}, clock};
    ui::AnimationContext animations{owner.dispatcher()};
    float value = 0.0f;
    const auto handle = animations.start_tween(
        0.0f,
        1.0f,
        ui::DispatcherDuration{1.0},
        ui::Easing::Linear,
        ui::AnimationInvalidation::Paint,
        ui::AnimationInvalidationTarget{probe->invalidate_paint, probe->invalidate_layout},
        [&](float next) { value = next; });
    NUI_CHECK(handle.valid());

    visible.set(false);
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    clock->advance(ui::DispatcherDuration{0.5});
    NUI_CHECK(owner.checkpoint() == 1);
    NUI_CHECK(value > 0.0f);
    NUI_CHECK(!tree.dirty());
    NUI_CHECK(animations.cancel(handle));
}

void retained_overlay_dialog_removal_contract() {
    test::MockPlatform platform;
    SkCanvas canvas;
    ui::UI tree{ui::Spacer{200.0f, 100.0f}};
    tree.resize({200.0f, 100.0f});
    tree.activate(platform);
    tree.paint(canvas, platform);

    auto overlay_probe = std::make_shared<InvalidationProbeState>();
    ui::OverlaySpec overlay;
    overlay.placement = ui::OverlayPlacement::Center;
    overlay.content = ui::make_spec(InvalidationProbe{overlay_probe});
    const auto overlay_handle = tree.show_overlay(std::move(overlay));
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(overlay_handle.valid());
    NUI_CHECK(overlay_probe->node_id != ui::kInvalidNodeId);

    const auto overlay_stale = *overlay_probe;
    NUI_CHECK(tree.close_overlay(overlay_handle));
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());
    invoke_retained_invalidators(overlay_stale);
    NUI_CHECK(!tree.dirty());

    auto dialog_probe = std::make_shared<InvalidationProbeState>();
    int completions = 0;
    ui::Dialog dialog{tree};
    ui::DialogSpec spec;
    spec.title = "Lifetime";
    spec.body = ui::make_spec(InvalidationProbe{dialog_probe});
    NUI_CHECK(dialog.show(
                  std::move(spec),
                  [&](ui::DialogResult) { ++completions; }) == ui::DialogShowResult::Shown);
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(dialog_probe->node_id != ui::kInvalidNodeId);

    const auto dialog_stale = *dialog_probe;
    NUI_CHECK(dialog.close());
    tree.resize({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(completions == 1);
    NUI_CHECK(!tree.dirty());
    invoke_retained_invalidators(dialog_stale);
    NUI_CHECK(!tree.dirty());
}

void retained_virtual_list_recycling_contract() {
    using Runtime = ui::detail::VirtualListRetainedRuntime<int>;

    auto probe = std::make_shared<InvalidationProbeState>();
    auto runtime = std::make_shared<Runtime>(
        20.0f,
        [probe](const Runtime::Item& item) {
            if (item.key == 0) return ui::make_spec(InvalidationProbe{probe});
            return ui::make_spec(ui::Spacer{100.0f, 20.0f});
        });

    std::vector<Runtime::Item> items;
    items.reserve(100);
    for (int index = 0; index < 100; ++index) {
        items.emplace_back(index, "row");
    }
    NUI_CHECK(runtime->replace(std::move(items)));

    ui::UI tree{SpecRoot{ui::detail::make_virtual_list_retained_spec(runtime)}};
    ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());
    NUI_CHECK(probe->node_id != ui::kInvalidNodeId);

    const auto removed_id = probe->node_id;
    const auto stale = *probe;
    runtime->scroll().set_offset({0.0f, 400.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());
    invoke_retained_invalidators(stale);
    NUI_CHECK(!tree.dirty());

    runtime->scroll().set_offset({0.0f, 0.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(probe->node_id != ui::kInvalidNodeId);
    NUI_CHECK(probe->node_id != removed_id);
    NUI_CHECK(!tree.dirty());
    invoke_retained_invalidators(stale);
    NUI_CHECK(!tree.dirty());
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

        probe->invalidate_paint();
        NUI_CHECK(exposures.size() == 1);

        tree.paint(canvas, platform);
        NUI_CHECK(!tree.dirty());
        NUI_CHECK(probe->measures == measures_after_layout);

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

        exposures.clear();
        tree.paint(canvas, platform);
        NUI_CHECK(exposures.empty());
    }

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

    retained_invalidator_stale_node_contract();
    retained_invalidator_tree_lifetime_contract();
    retained_animation_target_removal_contract();
    retained_overlay_dialog_removal_contract();
    retained_virtual_list_recycling_contract();
    theme_change_classification();
    ui_theme_ownership_and_invalidation();
}

} // namespace

int main() { return test::run("invalidation", &suite); }
