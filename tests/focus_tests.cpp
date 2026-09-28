#include "test_support.hpp"

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

void suite() {
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
