#include "test_support.hpp"

#include "include/core/SkCanvas.h"

#include <functional>

namespace ui {

struct TreeTestAccess {
    // Simulate context-preparation failure after Commit without process-global
    // allocation hooks. The actual old blur and layout run through public APIs;
    // this seam only publishes the retained state preceding focus_changed(true).
    static void pause_before_focus_callback(Tree& tree) {
        NUI_CHECK(tree.focus_transition_stack_.size() == 1);
        auto& transition = tree.focus_transition_stack_.back();
        NUI_CHECK(transition.phase == Tree::FocusTransitionPhase::Commit);
        auto* next = tree.focus_transition_node(transition.next);
        const auto found = std::find(tree.focusables_.begin(), tree.focusables_.end(), next);
        NUI_CHECK(found != tree.focusables_.end());
        tree.focused_index_ = static_cast<std::size_t>(
            std::distance(tree.focusables_.begin(), found));
        tree.focus_active_ = true;
        transition.phase = Tree::FocusTransitionPhase::FocusNew;
    }
};

} // namespace ui

namespace {

struct PersistentBlurProbeState {
    int focus_in{};
    int focus_out{};
    int key_down{};
    bool throw_on_focus_in{};
    bool throw_on_focus_out{true};
};

class PersistentBlurProbeComponent final : public ui::Component {
public:
    explicit PersistentBlurProbeComponent(std::shared_ptr<PersistentBlurProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {}

    void focus_changed(bool focused, ui::FocusContext&) override {
        if (focused) {
            ++state_->focus_in;
            if (state_->throw_on_focus_in) {
                throw std::runtime_error("persistent focus setup failure");
            }
            return;
        }
        ++state_->focus_out;
        if (state_->throw_on_focus_out) {
            throw std::runtime_error("persistent focus teardown failure");
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown && event.key == ui::Key::Space) {
            ++state_->key_down;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

private:
    std::shared_ptr<PersistentBlurProbeState> state_;
};

class PersistentBlurProbe {
public:
    explicit PersistentBlurProbe(std::shared_ptr<PersistentBlurProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<PersistentBlurProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<PersistentBlurProbeState> state_;
};

struct FocusWithinProbeState {
    int leave_calls{};
    bool throw_on_leave{};
};

class FocusWithinProbeComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit FocusWithinProbeComponent(std::shared_ptr<FocusWithinProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{} : children.front().preferred;
    }

    [[nodiscard]] ui::Size minimum_size(
        const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{} : children.front().minimum;
    }

    [[nodiscard]] ui::Constraints child_constraints(
        const ui::Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(
        ui::Rect bounds,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void paint(ui::PaintContext&) const override {}

    void retained_focus_within_changed(bool within, bool, ui::Dispatcher) override {
        if (within) return;
        ++state_->leave_calls;
        if (state_->throw_on_leave) {
            throw std::runtime_error("persistent focus-within teardown failure");
        }
    }

private:
    std::shared_ptr<FocusWithinProbeState> state_;
};

class FocusWithinProbe {
public:
    template <class Child>
    FocusWithinProbe(std::shared_ptr<FocusWithinProbeState> state, Child&& child)
        : state_(std::move(state)), child_(ui::make_spec(std::forward<Child>(child))) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        std::vector<ui::Spec> children;
        children.push_back(std::move(child_));
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<FocusWithinProbeComponent>(state);
            },
            std::move(children)};
    }

private:
    std::shared_ptr<FocusWithinProbeState> state_;
    ui::Spec child_;
};


struct GeometryFocusState {
    ui::Size preferred{100.0f, 40.0f};
    std::vector<ui::Rect> focused_bounds;
    int blurs{};
    int keys{};
    int pointer_downs{};
    int layouts{};
    bool throw_layout{};
    bool throw_focus{};
    std::function<void()> on_layout;
};

class GeometryFocusComponent final : public ui::Component {
public:
    explicit GeometryFocusComponent(std::shared_ptr<GeometryFocusState> state)
        : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return state_->preferred;
    }
    void layout_children(ui::Rect,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>&) const override {
        ++state_->layouts;
        const auto callback = state_->on_layout;
        if (callback) callback();
        if (state_->throw_layout) throw std::runtime_error("focus geometry layout failure");
    }
    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (!focused) {
            ++state_->blurs;
            return;
        }
        state_->focused_bounds.push_back(context.bounds());
        if (state_->throw_focus) throw std::runtime_error("focus geometry callback failure");
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_downs;
            return ui::EventResult::Handled;
        }
        if (event.type != ui::InputType::KeyDown) return ui::EventResult::Ignored;
        ++state_->keys;
        return ui::EventResult::Handled;
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<GeometryFocusState> state_;
};

ui::Spec geometry_focus(std::shared_ptr<GeometryFocusState> state) {
    return ui::Spec{[state = std::move(state)] {
        return std::make_unique<GeometryFocusComponent>(state);
    }, {}};
}

void focus_geometry_is_prepared_before_activation_and_refresh() {
    auto probe = std::make_shared<GeometryFocusState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{geometry_focus(probe)}))};
    test::MockPlatform platform;
    tree.mount();
    // No prior layout/resize: activation must prepare the initial viewport
    // before a component derives caret/scroll state from its focus bounds.
    tree.activate_focus(platform);
    NUI_CHECK(probe->focused_bounds.size() == 1);
    NUI_CHECK(!probe->focused_bounds.back().empty());
    NUI_CHECK_NEAR(probe->focused_bounds.back().w, 100.0f, 0.001f);

    probe->preferred.w = 160.0f;
    tree.invalidate_layout();
    tree.refresh_focus(platform);
    NUI_CHECK(probe->focused_bounds.size() == 2);
    NUI_CHECK_NEAR(probe->focused_bounds.back().w, 160.0f, 0.001f);

    const auto layouts = probe->layouts;
    tree.refresh_focus(platform);
    NUI_CHECK(probe->layouts == layouts); // a clean focus refresh does not relayout
    tree.deactivate_focus(platform);
}

void dynamic_focus_geometry_is_valid_at_every_retained_entry() {
    // Every entry below can reconcile a newly mounted focus scope before its
    // customary layout pass. Checking callback bounds catches the native path
    // without HeadlessRenderer::render's implicit resize masking the ordering.
    for (int entry = 0; entry < 7; ++entry) {
#if !defined(NATIVEUI_ENABLE_INSPECTOR)
        if (entry == 6) continue;
#endif
        ui::State<bool> present{false};
        ui::State<bool> active{true};
        auto probe = std::make_shared<GeometryFocusState>();
        ui::Tree tree{ui::compile(ui::make_spec(ui::Padding{
            12.0f, ui::If{present, ui::FocusScope{active, geometry_focus(probe)}.trap()}}))};
        test::MockPlatform platform;
        SkCanvas canvas;
        tree.mount();
        tree.layout({320.0f, 90.0f});
        tree.activate_focus(platform);
        NUI_CHECK(probe->focused_bounds.empty());

        for (int reopening = 0; reopening < 2; ++reopening) {
            present.set(true);
            switch (entry) {
            case 0: tree.refresh_focus(platform); break;
            case 1: tree.paint(canvas, platform); break;
            case 2: {
                ui::Painter painter{canvas};
                tree.paint_region(painter, platform, {0.0f, 0.0f, 320.0f, 90.0f});
                break;
            }
            case 3: tree.focus_next(platform); break;
            case 4: tree.focus_previous(platform); break;
            case 5: (void)tree.dispatch(test::key(ui::Key::Space), platform); break;
#if defined(NATIVEUI_ENABLE_INSPECTOR)
            case 6: (void)tree.paint_with_inspector_snapshot(canvas, platform); break;
#endif
            default: break;
            }
            NUI_CHECK(!probe->focused_bounds.empty());
            for (const auto bounds : probe->focused_bounds) {
                NUI_CHECK_NEAR(bounds.x, 12.0f, 0.001f);
                NUI_CHECK_NEAR(bounds.y, 12.0f, 0.001f);
                NUI_CHECK_NEAR(bounds.w, 296.0f, 0.001f);
                NUI_CHECK_NEAR(bounds.h, 66.0f, 0.001f);
            }
            present.set(false);
            tree.refresh_focus(platform);
            probe->focused_bounds.clear();
        }
        tree.deactivate_focus(platform);
    }
}

void availability_rehomes_focus_with_committed_geometry() {
    ui::State<ui::VisibilityMode> visible{ui::VisibilityMode::Visible};
    auto first = std::make_shared<GeometryFocusState>();
    auto second = std::make_shared<GeometryFocusState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
        ui::Visibility{visible, geometry_focus(first)}, geometry_focus(second)}
        .gap(0.0f)))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({320.0f, 90.0f});
    tree.activate_focus(platform);
    visible.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(second->focused_bounds.size() == 1);
    NUI_CHECK_NEAR(second->focused_bounds.back().x, 0.0f, 0.001f);
    NUI_CHECK_NEAR(second->focused_bounds.back().w, 100.0f, 0.001f);
    tree.deactivate_focus(platform);
}

void focus_layout_failures_preserve_callback_progress_and_instance_isolation() {
    // Initial activation failure must not publish a new owner or notify a blur
    // to a component whose focus-in never began.
    {
        auto probe = std::make_shared<GeometryFocusState>();
        probe->throw_layout = true;
        ui::Tree tree{ui::compile(geometry_focus(probe))};
        test::MockPlatform platform;
        tree.mount();
        bool threw = false;
        try { tree.activate_focus(platform); }
        catch (const std::runtime_error&) { threw = true; }
        NUI_CHECK(threw);
        NUI_CHECK(probe->focused_bounds.empty());
        NUI_CHECK(probe->blurs == 0);
        NUI_CHECK(tree.layout_dirty());
        probe->throw_layout = false;
        tree.activate_focus(platform);
        NUI_CHECK(probe->focused_bounds.size() == 1);
        NUI_CHECK(!probe->focused_bounds.back().empty());
        tree.deactivate_focus(platform);
    }

    auto first = std::make_shared<GeometryFocusState>();
    auto second = std::make_shared<GeometryFocusState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
        geometry_focus(first), geometry_focus(second)}.gap(4.0f)))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({320.0f, 90.0f});
    tree.activate_focus(platform);
    first->preferred.w = 160.0f;
    second->throw_layout = true;
    tree.invalidate_layout();

    const auto earlier_layouts = first->layouts;
    bool threw = false;
    try { tree.focus_next(platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw);
    NUI_CHECK(first->layouts > earlier_layouts); // an earlier sibling was visited
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(second->focused_bounds.empty());
    NUI_CHECK(tree.layout_dirty());

    // A failed retained transaction in one tree cannot stall another instance.
    ui::State<bool> independent_value{false};
    test::MockPlatform independent_platform;
    ui::UI independent{ui::Toggle{"Independent", independent_value}};
    independent.resize({200.0f, 60.0f});
    independent.activate(independent_platform);
    independent.dispatch(test::key(ui::Key::Space), independent_platform);
    NUI_CHECK(independent_value.get());

    // Retry the unstarted layout, then inject a distinct callback exception.
    // The next checkpoint must skip both already-started focus callbacks.
    second->throw_layout = false;
    second->throw_focus = true;
    threw = false;
    try { (void)tree.dispatch(test::key(ui::Key::Space), platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw);
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(second->focused_bounds.size() == 1);
    NUI_CHECK_NEAR(second->focused_bounds.back().x, 164.0f, 0.001f);
    NUI_CHECK(!tree.layout_dirty());
    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(second->focused_bounds.size() == 1);
    NUI_CHECK(second->keys == 1);
    tree.deactivate_focus(platform);
    independent.deactivate(independent_platform);
}

void layout_can_cancel_or_redirect_a_pending_focus_target() {
    {
        ui::State<bool> target_enabled{true};
        auto first = std::make_shared<GeometryFocusState>();
        auto target = std::make_shared<GeometryFocusState>();
        ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
            geometry_focus(first), ui::Enabled{target_enabled, geometry_focus(target)}}
            .gap(4.0f)))};
        test::MockPlatform platform;
        tree.mount();
        tree.layout({320.0f, 90.0f});
        tree.activate_focus(platform);
        bool armed = true;
        first->on_layout = [&] {
            if (!armed) return;
            armed = false;
            target_enabled.set(false);
        };
        tree.invalidate_layout();
        tree.focus_next(platform);
        NUI_CHECK(first->blurs == 1);
        NUI_CHECK(target->focused_bounds.empty());
        tree.focus_previous(platform);
        NUI_CHECK(first->focused_bounds.size() == 2);
        NUI_CHECK(!first->focused_bounds.back().empty());
        tree.deactivate_focus(platform);
    }

    {
        auto first = std::make_shared<GeometryFocusState>();
        auto second = std::make_shared<GeometryFocusState>();
        auto third = std::make_shared<GeometryFocusState>();
        ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
            geometry_focus(first), geometry_focus(second), geometry_focus(third)}
            .gap(4.0f)))};
        test::MockPlatform platform;
        tree.mount();
        tree.layout({420.0f, 90.0f});
        tree.activate_focus(platform);
        bool armed = true;
        first->on_layout = [&] {
            if (!armed) return;
            armed = false;
            tree.focus_previous(platform); // a durable request for the third node
            throw std::runtime_error("layout failed after requesting focus");
        };
        tree.invalidate_layout();
        bool threw = false;
        try { tree.focus_next(platform); }
        catch (const std::runtime_error&) { threw = true; }
        NUI_CHECK(threw);
        NUI_CHECK(first->blurs == 1);
        NUI_CHECK(second->focused_bounds.empty());
        NUI_CHECK(third->focused_bounds.empty());
        (void)tree.dispatch(test::key(ui::Key::Space), platform);
        NUI_CHECK(first->blurs == 1);
        NUI_CHECK(second->focused_bounds.size() == 1);
        NUI_CHECK(second->blurs == 1);
        NUI_CHECK(third->focused_bounds.size() == 1);
        NUI_CHECK_NEAR(third->focused_bounds.back().x, 208.0f, 0.001f);
        NUI_CHECK(third->keys == 1);
        tree.deactivate_focus(platform);
    }
}

void refresh_layout_reentrance_recovers_the_new_focus_owner() {
    auto first = std::make_shared<GeometryFocusState>();
    auto second = std::make_shared<GeometryFocusState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
        geometry_focus(first), geometry_focus(second)}.gap(4.0f)))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({320.0f, 90.0f});
    tree.activate_focus(platform);
    bool armed = true;
    first->on_layout = [&] {
        if (!armed) return;
        armed = false;
        tree.focus_next(platform);
    };
    tree.invalidate_layout();
    bool threw = false;
    try { tree.refresh_focus(platform); }
    catch (const std::logic_error&) { threw = true; }
    // Nested layout retains the existing explicit rejection contract. Recovery
    // finishes only the unstarted transfer and never refreshes the old owner.
    NUI_CHECK(threw);
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(first->focused_bounds.size() == 1);
    NUI_CHECK(second->focused_bounds.empty());
    NUI_CHECK(tree.layout_dirty());
    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(first->blurs == 1);
    NUI_CHECK(first->focused_bounds.size() == 1);
    NUI_CHECK(second->focused_bounds.size() == 1);
    NUI_CHECK_NEAR(second->focused_bounds.back().x, 104.0f, 0.001f);
    NUI_CHECK(second->keys == 1);
    tree.deactivate_focus(platform);
}

void layout_teardown_retires_focus_work_without_poisoning_reactivation() {
    // Commit, resumed FocusNew, direct refresh and pointer targeting are
    // distinct callback paths. The scope matches an active numeric modal.
    // Exercise both accepted non-destructive lifecycle operations, including
    // teardown followed by a throwing layout callback.
    for (int entry = 0; entry < 4; ++entry) {
        for (const bool unmount : {false, true}) {
            for (const bool throw_after_teardown : {false, true}) {
                auto first = std::make_shared<GeometryFocusState>();
                auto second = std::make_shared<GeometryFocusState>();
                ui::State<bool> scope_active{true};
                ui::Tree tree{ui::compile(ui::make_spec(ui::FocusScope{scope_active,
                    ui::Row{geometry_focus(first), geometry_focus(second)}.gap(4.0f)}))};
                test::MockPlatform platform;
                int global_keys = 0;
                tree.set_global_key_down_handler([&](const ui::InputEvent&) {
                    ++global_keys;
                    return ui::EventResult::Ignored;
                });
                tree.mount();
                tree.layout({320.0f, 90.0f});
                tree.activate_focus(platform);

                if (entry == 1) {
                    second->throw_layout = true;
                    tree.invalidate_layout();
                    bool threw = false;
                    try { tree.focus_next(platform); }
                    catch (const std::runtime_error&) { threw = true; }
                    NUI_CHECK(threw);
                    NUI_CHECK(first->blurs == 1);
                    NUI_CHECK(second->focused_bounds.empty());
                    second->throw_layout = false;
                    tree.layout({320.0f, 90.0f});
                    ui::TreeTestAccess::pause_before_focus_callback(tree);
                }

                bool armed = true;
                int teardowns = 0;
                first->on_layout = [&] {
                    if (!armed) return;
                    armed = false;
                    ++teardowns;
                    if (unmount) tree.unmount();
                    else tree.deactivate_focus(platform);
                    if (throw_after_teardown) {
                        throw std::runtime_error("layout failed after focus teardown");
                    }
                };
                tree.invalidate_layout();
                bool threw = false;
                try {
                    if (entry == 0) tree.focus_next(platform);
                    else if (entry == 1) {
                        (void)tree.dispatch(test::key(ui::Key::Space), platform);
                    } else if (entry == 2) tree.refresh_focus(platform);
                    else {
                        (void)tree.dispatch(
                            test::pointer(ui::InputType::PointerDown, 120.0f, 20.0f), platform);
                    }
                } catch (const std::runtime_error&) { threw = true; }
                NUI_CHECK(threw == throw_after_teardown);
                NUI_CHECK(teardowns == 1);
                NUI_CHECK(first->focused_bounds.size() == 1);
                NUI_CHECK(second->focused_bounds.empty());
                (void)tree.dispatch(test::key(ui::Key::Space), platform);
                NUI_CHECK(first->keys == 0);
                NUI_CHECK(second->keys == 0);
                NUI_CHECK(global_keys == 0);
                NUI_CHECK(first->pointer_downs == 0);
                NUI_CHECK(second->pointer_downs == 0);

                // The original retained tree is reusable. This also detects a
                // leaked focus running-depth guard that would silently stall
                // activation, traversal or pending focus after the teardown.
                tree.mount();
                tree.activate_focus(platform);
                tree.focus_next(platform);
                (void)tree.dispatch(test::key(ui::Key::Space), platform);
                NUI_CHECK(first->focused_bounds.size() == 2);
                NUI_CHECK(second->focused_bounds.size() == 1);
                NUI_CHECK(!second->focused_bounds.back().empty());
                NUI_CHECK(second->keys == 1);
                NUI_CHECK(teardowns == 1);
                tree.deactivate_focus(platform);
            }
        }
    }
}

void scope_reconciliation_stops_after_layout_deactivation() {
    ui::State<bool> first_active{false};
    ui::State<bool> second_active{false};
    auto first = std::make_shared<GeometryFocusState>();
    auto second = std::make_shared<GeometryFocusState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
        ui::FocusScope{first_active, geometry_focus(first)},
        ui::FocusScope{second_active, geometry_focus(second)}}.gap(4.0f)))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({320.0f, 90.0f});
    tree.activate_focus(platform);
    bool armed = true;
    first->on_layout = [&] {
        if (!armed) return;
        armed = false;
        tree.deactivate_focus(platform);
    };
    first_active.set(true);
    second_active.set(true);
    tree.invalidate_layout();
    tree.refresh_focus(platform);
    NUI_CHECK(first->focused_bounds.empty());
    NUI_CHECK(second->focused_bounds.empty());
    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(first->keys == 0);
    NUI_CHECK(second->keys == 0);

    // Teardown reset both scope caches. Neither the interrupted recursive pass
    // nor an inactive traversal may pre-activate a sibling and hide it from the
    // next real activation.
    tree.focus_next(platform);
    NUI_CHECK(first->focused_bounds.empty());
    NUI_CHECK(second->focused_bounds.empty());
    tree.activate_focus(platform);
    NUI_CHECK(first->focused_bounds.size() == 1);
    NUI_CHECK(second->focused_bounds.size() == 1);
    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(second->keys == 1);
    tree.deactivate_focus(platform);
}

void dynamic_focus_fallback_rechecks_the_platform_after_layout_teardown() {
    for (const bool unmount : {false, true}) {
        ui::State<int> branch{0};
        ui::State<bool> scope_active{true};
        auto previous = std::make_shared<GeometryFocusState>();
        auto replacement = std::make_shared<GeometryFocusState>();
        ui::Tree tree{ui::compile(ui::make_spec(ui::Switch<int>{branch}
            .when(0, geometry_focus(previous))
            .when(1, ui::FocusScope{scope_active, geometry_focus(replacement)})))};
        test::MockPlatform platform;
        tree.mount();
        tree.layout({320.0f, 90.0f});
        tree.activate_focus(platform);
        bool armed = true;
        replacement->on_layout = [&] {
            if (!armed) return;
            armed = false;
            if (unmount) tree.unmount();
            else tree.deactivate_focus(platform);
        };
        // One reconciliation removes the focused branch and activates a new
        // scope. Its layout teardown retires active_platform_ before the old
        // removed-focus fallback would use that borrowed platform again.
        branch.set(1);
        tree.refresh_focus(platform);
        NUI_CHECK(previous->focused_bounds.size() == 1);
        NUI_CHECK(replacement->focused_bounds.empty());
        (void)tree.dispatch(test::key(ui::Key::Space), platform);
        NUI_CHECK(replacement->keys == 0);
        tree.mount();
        tree.activate_focus(platform);
        NUI_CHECK(replacement->focused_bounds.size() == 1);
        (void)tree.dispatch(test::key(ui::Key::Space), platform);
        NUI_CHECK(replacement->keys == 1);
        tree.deactivate_focus(platform);
    }
}

void suite() {
    focus_geometry_is_prepared_before_activation_and_refresh();
    dynamic_focus_geometry_is_valid_at_every_retained_entry();
    availability_rehomes_focus_with_committed_geometry();
    focus_layout_failures_preserve_callback_progress_and_instance_isolation();
    layout_can_cancel_or_redirect_a_pending_focus_target();
    refresh_layout_reentrance_recovers_the_new_focus_owner();
    layout_teardown_retires_focus_work_without_poisoning_reactivation();
    scope_reconciliation_stops_after_layout_deactivation();
    dynamic_focus_fallback_rechecks_the_platform_after_layout_teardown();

    // Neutral input normalization: regular Tab keeps its modifier state while
    // AppKit BackTab U+0019 always means reverse traversal.
    {
        const auto tab = ui::detail::normalize_tab_key(0x09U, false);
        NUI_CHECK(tab.is_tab);
        NUI_CHECK(!tab.shift);

        const auto shifted_tab = ui::detail::normalize_tab_key(0x09U, true);
        NUI_CHECK(shifted_tab.is_tab);
        NUI_CHECK(shifted_tab.shift);

        const auto backtab = ui::detail::normalize_tab_key(0x19U, false);
        NUI_CHECK(backtab.is_tab);
        NUI_CHECK(backtab.shift);

        const auto unrelated = ui::detail::normalize_tab_key('x', false);
        NUI_CHECK(!unrelated.is_tab);
    }

    ui::State<bool> first{false};
    ui::State<bool> second{false};
    ui::State<bool> third{false};
    ui::UI tree{
        ui::Row{
            ui::Toggle{"First", first},
            ui::Toggle{"Second", second},
            ui::Toggle{"Third", third},
        }.gap(4.0f)
    };

    test::MockPlatform platform;
    tree.resize({720.0f, 100.0f});
    tree.activate(platform);

    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(first.get());
    NUI_CHECK(!second.get());
    NUI_CHECK(!third.get());

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(second.get());

    // Reverse one position: Second -> First.
    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!first.get());

    // Reverse wraps: First -> Third.
    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(third.get());

    // T125: once availability-driven focus teardown has entered application
    // code, an exception must not cause the same focus_changed(false) call to
    // be retried on the next dispatch. The callback deliberately keeps
    // throwing; recovery must complete retained bookkeeping, rehome focus to
    // the still-available sibling, and route that unrelated event normally.
    {
        ui::State<bool> enabled{true};
        ui::State<bool> fallback{false};
        auto probe = std::make_shared<PersistentBlurProbeState>();
        ui::UI recovery{
            ui::Row{
                ui::Enabled{enabled, PersistentBlurProbe{probe}},
                ui::Toggle{"Fallback", fallback}}
                .gap(4.0f)};

        test::MockPlatform recovery_platform;
        recovery.resize({240.0f, 80.0f});
        recovery.activate(recovery_platform);
        NUI_CHECK(probe->focus_in == 1);

        bool threw = false;
        try {
            enabled.set(false);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(probe->focus_out == 1);

        recovery.dispatch(test::key(ui::Key::Space), recovery_platform);
        NUI_CHECK(probe->focus_out == 1);
        NUI_CHECK(fallback.get());
    }

    // T125 closeout: after clear_focus publishes inactive focus, focus-within
    // leave observers must retain per-callback progress. A throwing inner
    // observer is entered once, the unstarted outer suffix does not run during
    // unwind, and the next safe dispatch completes that suffix without replay.
    {
        ui::State<bool> enabled{true};
        ui::State<bool> fallback_enabled{false};
        ui::State<bool> fallback{false};
        auto focus = std::make_shared<PersistentBlurProbeState>();
        focus->throw_on_focus_out = false;
        auto inner = std::make_shared<FocusWithinProbeState>();
        inner->throw_on_leave = true;
        auto outer = std::make_shared<FocusWithinProbeState>();

        ui::UI recovery{
            ui::Row{
                FocusWithinProbe{
                    outer,
                    FocusWithinProbe{
                        inner,
                        ui::Enabled{enabled, PersistentBlurProbe{focus}}}},
                ui::Enabled{fallback_enabled, ui::Toggle{"Fallback", fallback}}}
                .gap(4.0f)};

        test::MockPlatform recovery_platform;
        recovery.resize({260.0f, 80.0f});
        recovery.activate(recovery_platform);
        NUI_CHECK(focus->focus_in == 1);

        bool threw = false;
        try {
            enabled.set(false);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(focus->focus_out == 1);
        NUI_CHECK(inner->leave_calls == 1);
        NUI_CHECK(outer->leave_calls == 0);

        // This safe checkpoint resumes only the unstarted suffix. No callback
        // from the failed transition is executed during the preceding unwind.
        (void)recovery.dispatch(test::key(ui::Key::Space), recovery_platform);
        NUI_CHECK(inner->leave_calls == 1);
        NUI_CHECK(outer->leave_calls == 1);

        // The tree remains routable after semantic recovery.
        fallback_enabled.set(true);
        (void)recovery.dispatch(test::key(ui::Key::Tab), recovery_platform);
        (void)recovery.dispatch(test::key(ui::Key::Space), recovery_platform);
        NUI_CHECK(fallback.get());
    }

    // T125 B1: ordinary traversal uses the same no-retry recovery contract as
    // availability-driven focus loss. A persistently throwing old blur callback
    // is entered once; the next safe dispatch completes the prepared transition
    // and routes unrelated input to the new focus owner.
    {
        ui::State<bool> fallback{false};
        auto probe = std::make_shared<PersistentBlurProbeState>();
        ui::UI recovery{
            ui::Row{
                PersistentBlurProbe{probe},
                ui::Toggle{"Fallback", fallback}}
                .gap(4.0f)};

        test::MockPlatform recovery_platform;
        recovery.resize({240.0f, 80.0f});
        recovery.activate(recovery_platform);
        NUI_CHECK(probe->focus_in == 1);

        bool threw = false;
        try {
            (void)recovery.dispatch(test::key(ui::Key::Tab), recovery_platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(probe->focus_out == 1);

        (void)recovery.dispatch(test::key(ui::Key::Space), recovery_platform);
        NUI_CHECK(probe->focus_out == 1);
        NUI_CHECK(fallback.get());
    }

    // T125 B1: once the new focus owner has been published, a throwing focus-in
    // callback is not retried. The pending focus-within/descendant suffix is
    // completed at the next checkpoint and the already-published target remains
    // usable for unrelated keyboard input.
    {
        ui::State<bool> first_value{false};
        auto probe = std::make_shared<PersistentBlurProbeState>();
        probe->throw_on_focus_out = false;
        probe->throw_on_focus_in = true;
        ui::UI recovery{
            ui::Row{
                ui::Toggle{"First", first_value},
                PersistentBlurProbe{probe}}
                .gap(4.0f)};

        test::MockPlatform recovery_platform;
        recovery.resize({240.0f, 80.0f});
        recovery.activate(recovery_platform);

        bool threw = false;
        try {
            (void)recovery.dispatch(test::key(ui::Key::Tab), recovery_platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(probe->focus_in == 1);

        (void)recovery.dispatch(test::key(ui::Key::Space), recovery_platform);
        NUI_CHECK(probe->focus_in == 1);
        NUI_CHECK(probe->key_down == 1);
    }

    // A trapping active scope enters on its default focus and wraps both Tab directions.
    {
        ui::State<bool> active{true};
        ui::State<bool> first_in_scope{false};
        ui::State<bool> second_in_scope{false};
        ui::State<bool> outside{false};
        ui::UI scoped{
            ui::Column{
                ui::FocusScope{active,
                    ui::Row{
                        ui::Toggle{"Scoped A", first_in_scope},
                        ui::Toggle{"Scoped B", second_in_scope}}
                        .gap(4.0f)}
                    .trap(true)
                    .default_focus(1),
                ui::Toggle{"Outside", outside}}
                .gap(4.0f)
                .padding(0.0f)};

        test::MockPlatform scoped_platform;
        scoped.resize({720.0f, 160.0f});
        scoped.activate(scoped_platform);

        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!first_in_scope.get());
        NUI_CHECK(second_in_scope.get());
        NUI_CHECK(!outside.get());

        // Default second -> Tab wraps to first instead of escaping to Outside.
        scoped.dispatch(test::key(ui::Key::Tab), scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(first_in_scope.get());
        NUI_CHECK(!outside.get());

        // First -> Shift+Tab wraps back to second at the scope boundary.
        scoped.dispatch(test::key(ui::Key::Tab, true), scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!second_in_scope.get());
        NUI_CHECK(!outside.get());

        // Tree lifecycle restarts the active scope and reapplies its default focus.
        scoped.deactivate(scoped_platform);
        scoped.activate(scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(second_in_scope.get());
        NUI_CHECK(!outside.get());
    }

    // Activating a scope remembers the previous focus; deactivation restores it.
    {
        ui::State<bool> dialog_active{false};
        ui::State<bool> outside{false};
        ui::State<bool> dialog_a{false};
        ui::State<bool> dialog_b{false};
        ui::UI scoped{
            ui::Column{
                ui::Toggle{"Outside", outside},
                ui::FocusScope{dialog_active,
                    ui::Row{
                        ui::Toggle{"Dialog A", dialog_a},
                        ui::Toggle{"Dialog B", dialog_b}}
                        .gap(4.0f)}
                    .trap(true)
                    .default_focus(1)}
                .gap(4.0f)
                .padding(0.0f)};

        test::MockPlatform scoped_platform;
        scoped.resize({720.0f, 160.0f});
        scoped.activate(scoped_platform);

        // Inactive scope is skipped, so Outside owns initial focus.
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(outside.get());
        NUI_CHECK(!dialog_a.get());
        NUI_CHECK(!dialog_b.get());

        dialog_active.set(true);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(dialog_b.get());

        dialog_active.set(false);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!outside.get()); // restored to Outside and toggled again
        NUI_CHECK(dialog_b.get());
    }

    // T059: making an active trapping scope unavailable must preserve the scope's
    // existing restore target, not fall back to the first focusable in the tree.
    {
        ui::State<bool> dialog_active{false};
        ui::State<bool> scope_enabled{true};
        ui::State<bool> outside_a{false};
        ui::State<bool> outside_b{false};
        ui::State<bool> dialog_value{false};
        ui::UI scoped{
            ui::Column{
                ui::Toggle{"Outside A", outside_a},
                ui::Toggle{"Outside B", outside_b},
                ui::Enabled{scope_enabled,
                    ui::FocusScope{dialog_active,
                        ui::Toggle{"Dialog", dialog_value}}
                        .trap(true)}
            }.gap(4.0f).padding(0.0f)};

        test::MockPlatform scoped_platform;
        scoped.resize({720.0f, 180.0f});
        scoped.activate(scoped_platform);

        // Move the pre-dialog focus away from the first global focusable so an
        // incorrect generic fallback is observable.
        scoped.dispatch(test::key(ui::Key::Tab), scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(outside_b.get());
        NUI_CHECK(!outside_a.get());

        dialog_active.set(true);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(dialog_value.get());

        scope_enabled.set(false);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);

        // Existing FocusScope semantics restore Outside B. Falling back to the
        // first global focusable would toggle Outside A instead.
        NUI_CHECK(!outside_b.get());
        NUI_CHECK(!outside_a.get());
        NUI_CHECK(dialog_value.get());
    }

    // A deactivated tree must not route stray key events or traversal.
    tree.deactivate(platform);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(third.get());
}

} // namespace

int main() { return test::run("focus", &suite); }
