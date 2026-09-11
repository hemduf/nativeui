#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/interaction_observer.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

struct RouteState {
    int key_events{};
    int pointer_down{};
    int pointer_move{};
    ui::EventResult result{ui::EventResult::Ignored};
    bool capture_on_down{};
};

class RouteContainerComponent final : public ui::Component {
public:
    explicit RouteContainerComponent(std::shared_ptr<RouteState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{120.0f, 60.0f} : children.front().preferred;
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::KeyDown) ++state_->key_events;
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_down;
            if (state_->capture_on_down) context.capture_pointer();
        }
        if (event.type == ui::InputType::PointerMove) ++state_->pointer_move;
        return state_->result;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<RouteState> state_;
};

class RouteContainer {
public:
    template <class Child>
    RouteContainer(std::shared_ptr<RouteState> state, Child&& child)
        : state_(std::move(state)) {
        children_.push_back(ui::make_spec(std::forward<Child>(child)));
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<RouteContainerComponent>(state);
            },
            std::move(children_)};
    }

private:
    std::shared_ptr<RouteState> state_;
    std::vector<ui::Spec> children_;
};

struct InteractionState {
    int hover_in{};
    int hover_out{};
    int focus_in{};
    int focus_out{};
    bool hover_dispatcher_valid{};
    bool focus_dispatcher_valid{};
};

class InteractionObserverComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit InteractionObserverComponent(std::shared_ptr<InteractionState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{} : children.front().preferred;
    }

    [[nodiscard]] ui::Constraints child_constraints(
        const ui::Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void retained_pointer_hover_changed(bool hovered, ui::Dispatcher dispatcher) override {
        hovered ? ++state_->hover_in : ++state_->hover_out;
        state_->hover_dispatcher_valid = dispatcher.valid();
    }

    void retained_focus_within_changed(bool focused, ui::Dispatcher dispatcher) override {
        focused ? ++state_->focus_in : ++state_->focus_out;
        state_->focus_dispatcher_valid = dispatcher.valid();
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<InteractionState> state_;
};

class InteractionObserver {
public:
    template <class Child>
    InteractionObserver(std::shared_ptr<InteractionState> state, Child&& child)
        : state_(std::move(state)) {
        children_.push_back(ui::make_spec(std::forward<Child>(child)));
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<InteractionObserverComponent>(state);
            },
            std::move(children_)};
    }

private:
    std::shared_ptr<InteractionState> state_;
    std::vector<ui::Spec> children_;
};

void suite() {
    // Existing result and tree-owned Tab semantics.
    {
        auto probe = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{probe}};
        test::MockPlatform platform;
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        probe->input_result = ui::EventResult::Ignored;
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Ignored);
        NUI_CHECK(probe->key_events == 1);

        probe->input_result = ui::EventResult::Handled;
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
        NUI_CHECK(probe->key_events == 2);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) == ui::EventResult::Handled);
        NUI_CHECK(probe->key_events == 2);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 40.0f, 40.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 300.0f, 300.0f), platform) ==
                  ui::EventResult::Ignored);

        tree.deactivate(platform);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Ignored);
    }

    // Ignored leaf input bubbles to ancestors, nearest first.
    {
        auto child = std::make_shared<test::ProbeState>();
        auto parent = std::make_shared<RouteState>();
        auto root = std::make_shared<RouteState>();
        parent->result = ui::EventResult::Ignored;
        root->result = ui::EventResult::Handled;

        ui::UI tree{
            RouteContainer{root,
                RouteContainer{parent,
                    test::Probe{child}}}};
        test::MockPlatform platform;
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        child->input_result = ui::EventResult::Ignored;
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
        NUI_CHECK(child->key_events == 1);
        NUI_CHECK(parent->key_events == 1);
        NUI_CHECK(root->key_events == 1);
    }

    // A handled child stops propagation before any ancestor sees the event.
    {
        auto child = std::make_shared<test::ProbeState>();
        auto parent = std::make_shared<RouteState>();
        parent->result = ui::EventResult::Handled;
        child->input_result = ui::EventResult::Handled;

        ui::UI tree{RouteContainer{parent, test::Probe{child}}};
        test::MockPlatform platform;
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
        NUI_CHECK(child->key_events == 1);
        NUI_CHECK(parent->key_events == 0);
    }

    // Pointer capture remains authoritative: a captured leaf starts the route
    // even when the pointer later moves outside its hit-test bounds.
    {
        auto child = std::make_shared<RouteState>();
        auto parent = std::make_shared<RouteState>();
        child->capture_on_down = true;
        child->result = ui::EventResult::Handled;
        parent->result = ui::EventResult::Handled;

        class CapturingLeaf final : public ui::Component {
        public:
            explicit CapturingLeaf(std::shared_ptr<RouteState> state) : state_(std::move(state)) {}
            [[nodiscard]] bool focusable() const noexcept override { return true; }
            [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
                return {80.0f, 40.0f};
            }
            ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
                if (event.type == ui::InputType::PointerDown) {
                    ++state_->pointer_down;
                    context.capture_pointer();
                }
                if (event.type == ui::InputType::PointerMove) {
                    ++state_->pointer_move;
                    return ui::EventResult::Ignored;
                }
                return state_->result;
            }
            void paint(ui::PaintContext&) const override {}
        private:
            std::shared_ptr<RouteState> state_;
        };

        class CapturingLeafSpec {
        public:
            explicit CapturingLeafSpec(std::shared_ptr<RouteState> state) : state_(std::move(state)) {}
            ui::Spec spec() && {
                auto state = std::move(state_);
                return ui::Spec{[state] { return std::make_unique<CapturingLeaf>(state); }, {}};
            }
        private:
            std::shared_ptr<RouteState> state_;
        };

        ui::UI tree{RouteContainer{parent, CapturingLeafSpec{child}}};
        test::MockPlatform platform;
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(child->pointer_down == 1);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(child->pointer_move == 1);
        NUI_CHECK(parent->pointer_move == 1);
    }

    // T062 needs retained hover/focus observation, not event bubbling. A child
    // may consume PointerMove/keyboard input while its decorator still observes
    // route entry/exit and focus-within transitions. Both hooks also receive the
    // owning platform Dispatcher used for the exact tooltip delay contract.
    {
        ui::detail::DispatcherOwner dispatcher_owner;
        test::MockPlatform platform;
        platform.dispatcher_value = dispatcher_owner.dispatcher();

        auto observed = std::make_shared<InteractionState>();
        auto first = std::make_shared<test::ProbeState>();
        auto second = std::make_shared<test::ProbeState>();
        first->input_result = ui::EventResult::Handled;

        ui::UI tree{ui::Column{
            InteractionObserver{observed, test::Probe{first}},
            test::Probe{second},
        }};
        tree.resize({160.0f, 100.0f});
        tree.activate(platform);

        NUI_CHECK(observed->focus_in == 1);
        NUI_CHECK(observed->focus_out == 0);
        NUI_CHECK(observed->focus_dispatcher_valid);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(observed->hover_in == 1);
        NUI_CHECK(observed->hover_out == 0);
        NUI_CHECK(observed->hover_dispatcher_valid);

        // Remaining inside the same retained route must not re-notify/restart.
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 22.0f, 22.0f), platform);
        NUI_CHECK(observed->hover_in == 1);

        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 80.0f), platform);
        NUI_CHECK(observed->hover_out == 1);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) == ui::EventResult::Handled);
        NUI_CHECK(observed->focus_out == 1);
    }
}

} // namespace

int main() { return test::run("routing", &suite); }
