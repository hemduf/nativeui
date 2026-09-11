#include "test_support.hpp"

#include <functional>
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

struct CloseFromInputState {
    std::function<bool()> close;
    bool close_result{};
    bool returned_from_input{};
    bool unmounted_during_input{};
    bool completion_after_input{};
    bool completion_after_unmount{};
    int unmounts{};
};

class CloseFromInputComponent final : public ui::Component {
public:
    explicit CloseFromInputComponent(std::shared_ptr<CloseFromInputState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 40.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        auto state = state_;
        state->close_result = state->close();
        state->unmounted_during_input = state->unmounts != 0;
        state->returned_from_input = true;
        return ui::EventResult::Handled;
    }

    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<CloseFromInputState> state_;
};

class CloseFromInputBody {
public:
    explicit CloseFromInputBody(std::shared_ptr<CloseFromInputState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<CloseFromInputComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<CloseFromInputState> state_;
};

ui::DialogSpec dialog_spec() {
    ui::DialogSpec spec;
    spec.title = "Confirm";
    spec.body = ui::make_spec(ui::Spacer{80.0f, 40.0f});
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});
    return spec;
}

ui::DialogSpec dialog_spec(std::shared_ptr<CloseFromInputState> state) {
    auto spec = dialog_spec();
    spec.body = ui::make_spec(CloseFromInputBody{std::move(state)});
    return spec;
}

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

    // T063 is policy over T061: while active, the modal barrier owns pointer
    // input outside the centered dialog and underlying content cannot observe
    // it. Programmatic close removes that barrier and root input resumes.
    {
        auto child = std::make_shared<test::ProbeState>();
        child->input_result = ui::EventResult::Ignored;
        auto root = std::make_shared<RouteState>();
        root->result = ui::EventResult::Handled;
        ui::UI tree{RouteContainer{root, test::Probe{child}}};
        test::MockPlatform platform;
        tree.resize({200.0f, 120.0f});
        tree.activate(platform);

        ui::Dialog dialog{tree};
        NUI_CHECK(dialog.show(dialog_spec(), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Shown);
        tree.resize({200.0f, 120.0f});

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 4.0f, 4.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(root->pointer_down == 0);

        NUI_CHECK(dialog.close());
        tree.resize({200.0f, 120.0f});
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 40.0f, 40.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(root->pointer_down == 1);
    }

    // Closing from the dialog subtree must only mark logical completion while
    // the current input callback is on the stack. T058 teardown happens at the
    // outer dispatch checkpoint; only then may the application completion run.
    {
        test::MockPlatform platform;
        ui::UI tree{ui::Spacer{200.0f, 120.0f}};
        tree.resize({200.0f, 120.0f});
        tree.activate(platform);

        auto state = std::make_shared<CloseFromInputState>();
        ui::Dialog first{tree};
        ui::Dialog second{tree};
        ui::DialogShowResult reentrant_show = ui::DialogShowResult::Unavailable;

        NUI_CHECK(first.show(dialog_spec(state), [&](ui::DialogResult result) {
            NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
            state->completion_after_input = state->returned_from_input;
            state->completion_after_unmount = state->unmounts == 1;
            reentrant_show = second.show(dialog_spec(), [](ui::DialogResult) {});
        }) == ui::DialogShowResult::Shown);
        state->close = [&first] { return first.close(); };
        tree.resize({200.0f, 120.0f});

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 100.0f, 60.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->close_result);
        NUI_CHECK(state->returned_from_input);
        NUI_CHECK(!state->unmounted_during_input);
        NUI_CHECK(state->unmounts == 1);
        NUI_CHECK(state->completion_after_input);
        NUI_CHECK(state->completion_after_unmount);
        NUI_CHECK(reentrant_show == ui::DialogShowResult::Shown);
        NUI_CHECK(second.close());
    }
}

} // namespace

int main() { return test::run("routing", &suite); }
