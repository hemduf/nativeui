#define main nativeui_base_routing_main
#include "routing_tests_base.inc"
#undef main

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/interaction_observer.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

struct T062InteractionState {
    int hover_in{};
    int hover_out{};
    int focus_in{};
    int focus_out{};
    bool hover_dispatcher_valid{};
    bool focus_dispatcher_valid{};
};

class T062InteractionObserverComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit T062InteractionObserverComponent(std::shared_ptr<T062InteractionState> state)
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
    std::shared_ptr<T062InteractionState> state_;
};

class T062InteractionObserver {
public:
    template <class Child>
    T062InteractionObserver(std::shared_ptr<T062InteractionState> state, Child&& child)
        : state_(std::move(state)) {
        children_.push_back(ui::make_spec(std::forward<Child>(child)));
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<T062InteractionObserverComponent>(state);
            },
            std::move(children_)};
    }

private:
    std::shared_ptr<T062InteractionState> state_;
    std::vector<ui::Spec> children_;
};

void t062_interaction_observer_suite() {
    ui::detail::DispatcherOwner dispatcher_owner;
    test::MockPlatform platform;
    platform.dispatcher_value = dispatcher_owner.dispatcher();

    auto observed = std::make_shared<T062InteractionState>();
    auto first = std::make_shared<test::ProbeState>();
    auto second = std::make_shared<test::ProbeState>();
    first->input_result = ui::EventResult::Handled;

    ui::UI tree{ui::Column{
        T062InteractionObserver{observed, test::Probe{first}},
        test::Probe{second},
    }};
    tree.resize({160.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK(observed->focus_in == 1);
    NUI_CHECK(observed->focus_out == 0);
    NUI_CHECK(observed->focus_dispatcher_valid);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 40.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(observed->hover_in == 1);
    NUI_CHECK(observed->hover_out == 0);
    NUI_CHECK(observed->hover_dispatcher_valid);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 42.0f, 42.0f), platform);
    NUI_CHECK(observed->hover_in == 1);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 40.0f, 90.0f), platform);
    NUI_CHECK(observed->hover_out == 1);

    NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) == ui::EventResult::Handled);
    NUI_CHECK(observed->focus_out == 1);
}

} // namespace

int main() {
    const int base_result = nativeui_base_routing_main();
    if (base_result != EXIT_SUCCESS) return base_result;
    return test::run("t062 interaction observer", &t062_interaction_observer_suite);
}
