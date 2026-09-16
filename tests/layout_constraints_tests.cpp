#include "test_support.hpp"

#include "include/core/SkCanvas.h"

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void check_size(ui::Size actual, ui::Size expected) {
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.0001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.0001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

enum class LayoutFault {
    None,
    Measure,
    ChildConstraints,
    RootLayoutChildren,
    SecondChildLayoutChildren,
};

struct LayoutFaultState {
    LayoutFault fault{LayoutFault::None};
    int first_pointer_downs{};
    bool reenter_layout_from_root{};
    bool reentrant_layout_started{};
    int reentrant_layout_calls{};
    std::function<void()> reentrant_layout;
};

class LayoutFaultLeaf final : public ui::Component {
public:
    LayoutFaultLeaf(std::shared_ptr<LayoutFaultState> state, bool first)
        : state_(std::move(state)), first_(first) {}

    [[nodiscard]] bool focusable() const noexcept override { return first_; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        if (!first_ && state_->fault == LayoutFault::Measure) {
            throw std::runtime_error("layout measure fault");
        }
        return {100.0f, 40.0f};
    }

    void layout_children(
        ui::Rect,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>&) const override {
        if (!first_ && state_->fault == LayoutFault::SecondChildLayoutChildren) {
            throw std::runtime_error("second child layout fault");
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (first_ && event.type == ui::InputType::PointerDown) {
            ++state_->first_pointer_downs;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LayoutFaultState> state_;
    bool first_{};
};

class LayoutFaultRoot final : public ui::Component {
public:
    explicit LayoutFaultRoot(std::shared_ptr<LayoutFaultState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200.0f, 100.0f};
    }

    [[nodiscard]] ui::Constraints child_constraints(
        const ui::Constraints& constraints,
        std::size_t index,
        std::size_t) const override {
        if (index == 1 && state_->fault == LayoutFault::ChildConstraints) {
            throw std::runtime_error("child constraints fault");
        }
        return constraints.loosen();
    }

    void layout_children(
        ui::Rect bounds,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (state_->reenter_layout_from_root && !state_->reentrant_layout_started &&
            state_->reentrant_layout) {
            state_->reentrant_layout_started = true;
            ++state_->reentrant_layout_calls;
            state_->reentrant_layout();
        }
        if (state_->fault == LayoutFault::RootLayoutChildren) {
            throw std::runtime_error("root layout fault");
        }
        if (placements.size() < 2) return;
        const float left_width = bounds.w * 0.5f;
        placements[0].bounds = {bounds.x, bounds.y, left_width, bounds.h};
        placements[1].bounds = {
            bounds.x + left_width, bounds.y, bounds.w - left_width, bounds.h};
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LayoutFaultState> state_;
};

class LayoutFaultFixture {
public:
    explicit LayoutFaultFixture(std::shared_ptr<LayoutFaultState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        ui::Spec root{
            [state] { return std::make_unique<LayoutFaultRoot>(state); },
            {}};
        root.children.push_back(ui::Spec{
            [state] { return std::make_unique<LayoutFaultLeaf>(state, true); },
            {}});
        root.children.push_back(ui::Spec{
            [state] { return std::make_unique<LayoutFaultLeaf>(state, false); },
            {}});
        return root;
    }

private:
    std::shared_ptr<LayoutFaultState> state_;
};

struct LifecycleRollbackReentryState {
    bool throw_activate{};
    int measures{};
    int paints{};
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
    int destroyed{};
    int rollback_callbacks{};
    bool saw_nonempty_reentrant_measure{};
#if defined(NATIVEUI_ENABLE_INSPECTOR)
    std::size_t inspector_nodes_during_rollback{};
#endif
    std::function<void()> during_rollback;
};

class LifecycleRollbackReentryComponent final : public ui::Component {
public:
    explicit LifecycleRollbackReentryComponent(
        std::shared_ptr<LifecycleRollbackReentryState> state)
        : state_(std::move(state)) {}

    ~LifecycleRollbackReentryComponent() override { ++state_->destroyed; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        ++state_->measures;
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }

    void activate(ui::LifecycleContext&) override {
        ++state_->activates;
        if (state_->throw_activate) {
            throw std::runtime_error("t130 rollback activate fault");
        }
    }

    void deactivate(ui::LifecycleContext&) override {
        ++state_->deactivates;
        if (state_->throw_activate && state_->during_rollback) {
            ++state_->rollback_callbacks;
            state_->during_rollback();
        }
    }

    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }

    void paint(ui::PaintContext&) const override { ++state_->paints; }

private:
    std::shared_ptr<LifecycleRollbackReentryState> state_;
};

class LifecycleRollbackReentryProbe {
public:
    explicit LifecycleRollbackReentryProbe(
        std::shared_ptr<LifecycleRollbackReentryState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = state_;
        return ui::Spec{
            [state] { return std::make_unique<LifecycleRollbackReentryComponent>(state); },
            {}};
    }

private:
    std::shared_ptr<LifecycleRollbackReentryState> state_;
};

void lifecycle_rollback_public_entrypoint_contract() {
    // Direct Tree geometry entry points must not traverse provisional dynamic
    // children while lifecycle rollback journals still hold raw Node* entries.
    {
        ui::State<bool> visible{false};
        auto state = std::make_shared<LifecycleRollbackReentryState>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(
            ui::If{visible, LifecycleRollbackReentryProbe{state}}.spec())};
        tree.mount();
        tree.layout({160.0f, 80.0f});
        tree.activate_focus(platform);

        state->during_rollback = [&] {
            const auto metrics = tree.measure(ui::Constraints::unbounded());
            state->saw_nonempty_reentrant_measure =
                metrics.preferred.w != 0.0f || metrics.preferred.h != 0.0f;
            tree.layout({320.0f, 120.0f});
        };
        state->throw_activate = true;
        visible.set(true);

        bool threw = false;
        try {
            tree.layout({160.0f, 80.0f});
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()} == "t130 rollback activate fault";
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->rollback_callbacks == 1);
        NUI_CHECK(!state->saw_nonempty_reentrant_measure);
        NUI_CHECK(state->measures == 0);

        state->throw_activate = false;
        state->during_rollback = {};
        tree.layout({160.0f, 80.0f});
        NUI_CHECK(state->activates == 2);
        NUI_CHECK(state->measures > 0);

        tree.deactivate_focus(platform);
        tree.unmount();
    }

    // UI overlay geometry/paint plus inspector diagnostics obey the same
    // per-Tree transaction barrier. Work queued by the rollback callback must
    // survive for the next ordinary retained checkpoint, while another UI is
    // unaffected by the first UI's active lifecycle token.
    {
        ui::State<bool> visible{false};
        ui::State<bool> queued_visible{false};
        auto failing = std::make_shared<LifecycleRollbackReentryState>();
        auto queued = std::make_shared<LifecycleRollbackReentryState>();
        auto independent = std::make_shared<LifecycleRollbackReentryState>();
        test::MockPlatform platform;
        test::MockPlatform independent_platform;
        SkCanvas canvas;

        ui::UI tree{
            ui::Column{
                ui::If{visible, LifecycleRollbackReentryProbe{failing}},
                ui::If{queued_visible, LifecycleRollbackReentryProbe{queued}}
            }.gap(2.0f)};
        ui::UI second{LifecycleRollbackReentryProbe{independent}};

        tree.resize({160.0f, 80.0f});
        tree.activate(platform);
        second.resize({80.0f, 40.0f});
        second.activate(independent_platform);
        const int independent_measures_before = independent->measures;

        failing->during_rollback = [&] {
            queued_visible.set(true);
            const auto metrics = tree.measure(ui::Constraints::unbounded());
            failing->saw_nonempty_reentrant_measure =
                metrics.preferred.w != 0.0f || metrics.preferred.h != 0.0f;
            tree.resize({320.0f, 120.0f});
            tree.paint(canvas, platform);
#if defined(NATIVEUI_ENABLE_INSPECTOR)
            failing->inspector_nodes_during_rollback =
                ui::debug::inspector_snapshot(tree).nodes.size();
#endif
            (void)second.measure(ui::Constraints::unbounded());
        };
        failing->throw_activate = true;
        visible.set(true);

        bool threw = false;
        try {
            tree.resize({160.0f, 80.0f});
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()} == "t130 rollback activate fault";
        }
        NUI_CHECK(threw);
        NUI_CHECK(failing->rollback_callbacks == 1);
        NUI_CHECK(!failing->saw_nonempty_reentrant_measure);
        NUI_CHECK(failing->measures == 0);
        NUI_CHECK(failing->paints == 0);
        NUI_CHECK(queued->mounts == 0);
        NUI_CHECK(independent->measures > independent_measures_before);
#if defined(NATIVEUI_ENABLE_INSPECTOR)
        NUI_CHECK(failing->inspector_nodes_during_rollback == 0);
#endif

        failing->throw_activate = false;
        failing->during_rollback = {};
        tree.resize({160.0f, 80.0f});
        NUI_CHECK(failing->activates == 2);
        NUI_CHECK(queued->mounts == 1);
        NUI_CHECK(queued->activates == 1);
        NUI_CHECK(failing->measures > 0);

        tree.deactivate(platform);
        second.deactivate(independent_platform);
    }
}

void layout_exception_transaction_contract() {
    auto state = std::make_shared<LayoutFaultState>();
    ui::UI tree{LayoutFaultFixture{state}};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.resize({200.0f, 100.0f});
    tree.activate(platform);
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    // Preserve a pre-existing paint requirement across a failed resize. The
    // second child throws only after child 0 has already published its new
    // 150px-wide geometry inside the failed traversal, so rollback is observable
    // through pointer routing without any diagnostic/private-tree access.
    tree.invalidate({10.0f, 10.0f, 8.0f, 8.0f});
    NUI_CHECK(tree.dirty_regions().size() == 1);
    check_rect(tree.dirty_regions().front(), {10.0f, 10.0f, 8.0f, 8.0f});

    state->fault = LayoutFault::SecondChildLayoutChildren;
    bool threw = false;
    try {
        tree.resize({300.0f, 100.0f});
    } catch (const std::runtime_error& error) {
        threw = true;
        NUI_CHECK(std::string{error.what()} == "second child layout fault");
    }
    NUI_CHECK(threw);
    NUI_CHECK(tree.layout_dirty());
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(tree.dirty_regions().size() == 1);
    check_rect(tree.dirty_regions().front(), {10.0f, 10.0f, 8.0f, 8.0f});

    const auto old_geometry_result = tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 125.0f, 20.0f), platform);
    NUI_CHECK(old_geometry_result == ui::EventResult::Ignored);
    NUI_CHECK(state->first_pointer_downs == 0);

    state->fault = LayoutFault::None;
    tree.resize({300.0f, 100.0f});
    NUI_CHECK(!tree.layout_dirty());
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    const auto new_geometry_result = tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 125.0f, 20.0f), platform);
    NUI_CHECK(new_geometry_result == ui::EventResult::Handled);
    NUI_CHECK(state->first_pointer_downs == 1);
    (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 125.0f, 20.0f), platform);

    // The same UI instance must also recover from each callback seam named by
    // T130. Lazy paint-driven layout uses the same transaction as explicit
    // viewport layout and must retain dirty state until a later successful pass.
    for (const auto fault : {
             LayoutFault::Measure,
             LayoutFault::ChildConstraints,
             LayoutFault::RootLayoutChildren}) {
        state->fault = fault;
        tree.invalidate_layout();
        threw = false;
        try {
            tree.paint(canvas, platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(tree.layout_dirty());
        NUI_CHECK(tree.paint_dirty());

        state->fault = LayoutFault::None;
        tree.paint(canvas, platform);
        NUI_CHECK(!tree.layout_dirty());
        NUI_CHECK(!tree.paint_dirty());
    }

    // Layout callbacks are application code and may synchronously re-enter the
    // same UI. A lazy paint entry must reject that nested top-level layout rather
    // than joining the active rollback journal. The one-shot callback avoids an
    // unbounded recursive test failure if the guard regresses: without the guard
    // the inner paint returns and this assertion observes the missing rejection.
    state->reenter_layout_from_root = true;
    state->reentrant_layout_started = false;
    state->reentrant_layout_calls = 0;
    state->reentrant_layout = [&] { tree.paint(canvas, platform); };
    tree.invalidate_layout();
    threw = false;
    try {
        tree.paint(canvas, platform);
    } catch (const std::logic_error& error) {
        threw = true;
        NUI_CHECK(std::string{error.what()} == "reentrant retained layout transaction");
    }
    NUI_CHECK(threw);
    NUI_CHECK(state->reentrant_layout_calls == 1);
    NUI_CHECK(tree.layout_dirty());
    NUI_CHECK(tree.paint_dirty());

    state->reenter_layout_from_root = false;
    state->reentrant_layout = {};
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(!tree.paint_dirty());
}

void suite() {
    // Constraint sanitization/clamping.
    {
        const ui::Constraints c{{20.0f, 10.0f}, {100.0f, 80.0f}};
        check_size(c.constrain({5.0f, 200.0f}), {20.0f, 80.0f});
        check_size(c.constrain({50.0f, 40.0f}), {50.0f, 40.0f});

        const ui::Constraints bad{{-10.0f, std::nanf("")}, {-4.0f, 30.0f}};
        NUI_CHECK(bad.min.w >= 0.0f && bad.min.h >= 0.0f);
        NUI_CHECK(bad.max.w >= bad.min.w && bad.max.h >= bad.min.h);
        const auto clean = bad.constrain({std::nanf(""), -20.0f});
        NUI_CHECK(std::isfinite(clean.w) && clean.w >= 0.0f);
        NUI_CHECK(std::isfinite(clean.h) && clean.h >= 0.0f);
    }

    // Intrinsic preferred/minimum metrics remain distinguishable.
    {
        ui::SpacerComponent fixed{{80.0f, 30.0f}};
        const auto metrics = fixed.measure_constrained(ui::Constraints::loose({200.0f, 200.0f}), {});
        check_size(metrics.minimum, {80.0f, 30.0f});
        check_size(metrics.preferred, {80.0f, 30.0f});

        const auto clipped = fixed.measure_constrained(ui::Constraints::loose({40.0f, 20.0f}), {});
        check_size(clipped.minimum, {40.0f, 20.0f});
        check_size(clipped.preferred, {40.0f, 20.0f});
    }

    // Nested Row/Column preferred size is deterministically bounded by the
    // supplied constraints while preserving natural size when unbounded.
    {
        auto root = ui::compile(ui::make_spec(
            ui::Column{
                ui::Row{ui::Spacer{80.0f, 20.0f}, ui::Spacer{60.0f, 30.0f}}.gap(10.0f),
                ui::Spacer{30.0f, 15.0f},
            }.padding(5.0f).gap(5.0f)));
        ui::Tree tree{std::move(root)};
        tree.mount();

        const auto natural = tree.measure(ui::Constraints::unbounded());
        check_size(natural.preferred, {160.0f, 60.0f});

        const auto bounded = tree.measure(ui::Constraints::loose({100.0f, 50.0f}));
        check_size(bounded.preferred, {100.0f, 50.0f});
        NUI_CHECK(bounded.minimum.w <= bounded.preferred.w);
        NUI_CHECK(bounded.minimum.h <= bounded.preferred.h);
    }

    // Resize never creates negative/NaN component bounds even below natural size.
    {
        ui::UI tree{
            ui::Column{
                ui::Row{ui::Spacer{80.0f, 20.0f}, ui::Spacer{60.0f, 30.0f}}.gap(10.0f),
                ui::Spacer{30.0f, 15.0f},
            }.padding(5.0f).gap(5.0f)
        };
        tree.resize({37.0f, 29.0f});
        const auto metrics = tree.measure(ui::Constraints::tight({37.0f, 29.0f}));
        check_size(metrics.preferred, {37.0f, 29.0f});

        auto first = std::make_shared<test::ProbeState>();
        auto second = std::make_shared<test::ProbeState>();
        ui::UI tiny{ui::Row{test::Probe{first}, test::Probe{second}}.gap(18.0f)};
        test::MockPlatform platform;
        tiny.resize({5.0f, 5.0f});
        tiny.activate(platform);
        tiny.dispatch(test::key(ui::Key::Tab), platform);
        for (const auto& state : {first, second}) {
            NUI_CHECK(!state->focus_bounds.empty());
            const auto b = state->focus_bounds.back();
            NUI_CHECK(std::isfinite(b.x) && std::isfinite(b.y));
            NUI_CHECK(std::isfinite(b.w) && b.w >= 0.0f);
            NUI_CHECK(std::isfinite(b.h) && b.h >= 0.0f);
        }
    }

    lifecycle_rollback_public_entrypoint_contract();
    layout_exception_transaction_contract();
}

} // namespace

int main() { return test::run("layout_constraints", &suite); }
