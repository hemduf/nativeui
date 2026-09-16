#define main nativeui_base_routing_main
#include "routing_tests_base.inc"
#undef main

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/interaction_observer.hpp>

#include <memory>
#include <string>
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
    bool hover_interaction_active{};
    bool focus_interaction_active{};
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

    void retained_pointer_hover_changed(
        bool hovered, bool pointer_interaction_active, ui::Dispatcher dispatcher) override {
        hovered ? ++state_->hover_in : ++state_->hover_out;
        state_->hover_dispatcher_valid = dispatcher.valid();
        state_->hover_interaction_active = pointer_interaction_active;
    }

    void retained_focus_within_changed(
        bool focused, bool pointer_interaction_active, ui::Dispatcher dispatcher) override {
        focused ? ++state_->focus_in : ++state_->focus_out;
        state_->focus_dispatcher_valid = dispatcher.valid();
        state_->focus_interaction_active = pointer_interaction_active;
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

ui::InputEvent t174_composition(
    ui::CompositionType type,
    std::string text = {},
    std::size_t cursor_byte = 0,
    std::size_t selection_bytes = 0) {
    ui::InputEvent event{};
    event.type = ui::InputType::Composition;
    event.composition.type = type;
    event.composition.text = std::move(text);
    event.composition.cursor_byte = cursor_byte;
    event.composition.selection_bytes = selection_bytes;
    return event;
}

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
    NUI_CHECK(!observed->focus_interaction_active);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 40.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(observed->hover_in == 1);
    NUI_CHECK(observed->hover_out == 0);
    NUI_CHECK(observed->hover_dispatcher_valid);
    NUI_CHECK(!observed->hover_interaction_active);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 42.0f, 42.0f), platform);
    NUI_CHECK(observed->hover_in == 1);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 40.0f, 90.0f), platform);
    NUI_CHECK(observed->hover_out == 1);

    NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) == ui::EventResult::Handled);
    NUI_CHECK(observed->focus_out == 1);
}

void t174_key_down_fallback_suite() {
    // A focused text widget in one subtree may ignore an application shortcut;
    // the sibling subtree is not part of routing and the final per-UI fallback
    // receives the original raw KeyDown exactly once.
    {
        test::MockPlatform platform;
        ui::State<std::string> value{"hello"};
        auto sibling = std::make_shared<test::ProbeState>();
        ui::UI tree{ui::Column{
            ui::TextInput{"Name", value},
            test::Probe{sibling},
        }};
        tree.resize({320.0f, 120.0f});
        tree.activate(platform);

        int calls = 0;
        ui::InputEvent observed{};
        tree.set_key_down_handler([&](const ui::InputEvent& event) {
            ++calls;
            observed = event;
            return ui::EventResult::Handled;
        });

        const auto shortcut = test::key(ui::Key::G, false, true, true);
        NUI_CHECK(tree.dispatch(shortcut, platform) == ui::EventResult::Handled);
        NUI_CHECK(calls == 1);
        NUI_CHECK(observed.type == ui::InputType::KeyDown);
        NUI_CHECK(observed.key == ui::Key::G);
        NUI_CHECK(observed.primary);
        NUI_CHECK(observed.ctrl);
        NUI_CHECK(!observed.shift);
        NUI_CHECK(observed.alt);
        NUI_CHECK(sibling->key_events == 0);
        NUI_CHECK(value.get() == "hello");
    }

    // A handled focused component stops before the fallback.
    {
        test::MockPlatform platform;
        auto probe = std::make_shared<test::ProbeState>();
        probe->input_result = ui::EventResult::Handled;
        ui::UI tree{test::Probe{probe}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        int calls = 0;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return ui::EventResult::Handled;
        });
        NUI_CHECK(tree.dispatch(test::key(ui::Key::G), platform) == ui::EventResult::Handled);
        NUI_CHECK(probe->key_events == 1);
        NUI_CHECK(calls == 0);
    }

    // A handled ancestor likewise prevents the fallback.
    {
        test::MockPlatform platform;
        auto child = std::make_shared<test::ProbeState>();
        auto parent = std::make_shared<RouteState>();
        child->input_result = ui::EventResult::Ignored;
        parent->result = ui::EventResult::Handled;
        ui::UI tree{RouteContainer{parent, test::Probe{child}}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        int calls = 0;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return ui::EventResult::Handled;
        });
        NUI_CHECK(tree.dispatch(test::key(ui::Key::G), platform) == ui::EventResult::Handled);
        NUI_CHECK(parent->key_events == 1);
        NUI_CHECK(calls == 0);
    }

    // With no focused target, an eligible raw KeyDown reaches the fallback
    // directly. The callback result is the dispatch result and clearing the
    // handler restores the previous Ignored behavior.
    {
        test::MockPlatform platform;
        ui::UI tree{ui::Spacer{40.0f, 40.0f}};
        tree.resize({80.0f, 80.0f});
        tree.activate(platform);

        int calls = 0;
        ui::EventResult fallback_result = ui::EventResult::Ignored;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return fallback_result;
        });

        NUI_CHECK(tree.dispatch(test::key(ui::Key::G), platform) == ui::EventResult::Ignored);
        NUI_CHECK(calls == 1);
        fallback_result = ui::EventResult::Handled;
        NUI_CHECK(tree.dispatch(test::key(ui::Key::G), platform) == ui::EventResult::Handled);
        NUI_CHECK(calls == 2);

        tree.set_key_down_handler({});
        NUI_CHECK(tree.dispatch(test::key(ui::Key::G), platform) == ui::EventResult::Ignored);
        NUI_CHECK(calls == 2);
    }

    // Chords normalized to Command stay exclusively on the Command route even
    // when that route resolves to Ignored.
    {
        test::MockPlatform platform;
        auto probe = std::make_shared<test::ProbeState>();
        probe->input_result = ui::EventResult::Ignored;
        ui::UI tree{test::Probe{probe}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        int command_calls = 0;
        int key_calls = 0;
        tree.set_command_handler([&](ui::Command command) {
            ++command_calls;
            NUI_CHECK(command == ui::Command::SelectAll);
            return ui::EventResult::Ignored;
        });
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++key_calls;
            return ui::EventResult::Handled;
        });

        NUI_CHECK(tree.dispatch(test::key(ui::Key::A, false, true), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(command_calls == 1);
        NUI_CHECK(key_calls == 0);
        NUI_CHECK(probe->key_events == 0);
    }

    // Tree-owned Tab policy remains ahead of the fallback.
    {
        test::MockPlatform platform;
        auto probe = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{probe}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);
        int calls = 0;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return ui::EventResult::Handled;
        });
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) == ui::EventResult::Handled);
        NUI_CHECK(calls == 0);
    }

    // Only raw KeyDown is eligible for the fallback.
    {
        test::MockPlatform platform;
        ui::UI tree{ui::Spacer{40.0f, 40.0f}};
        tree.resize({80.0f, 80.0f});
        tree.activate(platform);
        int calls = 0;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return ui::EventResult::Handled;
        });

        auto key_up = test::key(ui::Key::G);
        key_up.type = ui::InputType::KeyUp;
        NUI_CHECK(tree.dispatch(key_up, platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(test::text("g"), platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(t174_composition(ui::CompositionType::Start), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(calls == 0);
    }

    // Installing the fallback must not alter IME composition delivery or
    // commit semantics for a focused TextInput.
    {
        test::MockPlatform platform;
        ui::State<std::string> value{"hello"};
        ui::UI tree{ui::TextInput{"Name", value}};
        tree.resize({320.0f, 90.0f});
        tree.activate(platform);
        int calls = 0;
        tree.set_key_down_handler([&](const ui::InputEvent&) {
            ++calls;
            return ui::EventResult::Handled;
        });

        tree.dispatch(t174_composition(ui::CompositionType::Start), platform);
        tree.dispatch(t174_composition(ui::CompositionType::Update, "日本", 6, 0), platform);
        NUI_CHECK(value.get() == "hello");
        tree.dispatch(t174_composition(ui::CompositionType::Commit, "日本"), platform);
        NUI_CHECK(value.get() == "hello日本");
        NUI_CHECK(calls == 0);
    }

    // Handler state belongs to each UI instance independently.
    {
        test::MockPlatform platform_a;
        test::MockPlatform platform_b;
        ui::UI first{ui::Spacer{40.0f, 40.0f}};
        ui::UI second{ui::Spacer{40.0f, 40.0f}};
        first.resize({80.0f, 80.0f});
        second.resize({80.0f, 80.0f});
        first.activate(platform_a);
        second.activate(platform_b);

        int first_calls = 0;
        int second_calls = 0;
        first.set_key_down_handler([&](const ui::InputEvent&) {
            ++first_calls;
            return ui::EventResult::Handled;
        });
        second.set_key_down_handler([&](const ui::InputEvent&) {
            ++second_calls;
            return ui::EventResult::Handled;
        });

        NUI_CHECK(first.dispatch(test::key(ui::Key::G), platform_a) == ui::EventResult::Handled);
        NUI_CHECK(first_calls == 1);
        NUI_CHECK(second_calls == 0);
        NUI_CHECK(second.dispatch(test::key(ui::Key::P), platform_b) == ui::EventResult::Handled);
        NUI_CHECK(first_calls == 1);
        NUI_CHECK(second_calls == 1);

        first.set_key_down_handler({});
        NUI_CHECK(second.dispatch(test::key(ui::Key::G), platform_b) == ui::EventResult::Handled);
        NUI_CHECK(second_calls == 2);
    }
}

} // namespace

int main() {
    const int base_result = nativeui_base_routing_main();
    if (base_result != EXIT_SUCCESS) return base_result;
    const int interaction_result =
        test::run("t062 interaction observer", &t062_interaction_observer_suite);
    if (interaction_result != EXIT_SUCCESS) return interaction_result;
    return test::run("t174 key down fallback", &t174_key_down_fallback_suite);
}
