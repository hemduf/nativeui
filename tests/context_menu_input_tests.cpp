#include "test_support.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

// T175: the right-button/context-menu request is a targeted input event. It
// must reach the pointer hit target with its position and modifiers, bubble
// through ancestors like other targeted input, leave focus untouched, cancel
// any stale active capture, and never establish a replacement capture.

struct MenuProbeState {
    int context_events{};
    int pointer_events{};
    int cancel_events{};
    int focus_in{};
    int focus_out{};
    ui::Point last_position{};
    bool shift{};
    bool primary{};
    bool capture_on_press{};
    bool capture_on_context{};
    bool throw_on_context{};
    bool throw_on_cancel{};
    ui::EventResult result{ui::EventResult::Ignored};
};

class MenuProbeComponent final : public ui::Component {
public:
    explicit MenuProbeComponent(std::shared_ptr<MenuProbeState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 80.0f};
    }

    void focus_changed(bool focused, ui::FocusContext&) override {
        focused ? ++state_->focus_in : ++state_->focus_out;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_events;
            if (state_->capture_on_press) context.capture_pointer();
        }
        if (event.type == ui::InputType::PointerCancel) {
            ++state_->cancel_events;
            if (state_->throw_on_cancel) throw std::runtime_error{"pointer-cancel test fault"};
        }
        if (event.type != ui::InputType::ContextMenu) return ui::EventResult::Ignored;
        ++state_->context_events;
        state_->last_position = event.position;
        state_->shift = event.shift;
        state_->primary = event.primary;
        if (state_->capture_on_context) context.capture_pointer();
        if (state_->throw_on_context) throw std::runtime_error{"context-menu test fault"};
        return state_->result;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<MenuProbeState> state_;
};

class MenuProbe {
public:
    explicit MenuProbe(std::shared_ptr<MenuProbeState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<MenuProbeComponent>(state); },
            {}};
    }

private:
    std::shared_ptr<MenuProbeState> state_;
};

struct MenuStageState {
    int context_events{};
    ui::EventResult result{ui::EventResult::Ignored};
};

// Lays out its two children as equal columns and records bubbled context
// events, so delivery and bubbling can be checked independently.
class MenuStageComponent final : public ui::Component {
public:
    explicit MenuStageComponent(std::shared_ptr<MenuStageState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {240.0f, 80.0f};
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        const float half = bounds.w * 0.5f;
        if (!placements.empty()) placements[0].bounds = {bounds.x, bounds.y, half, bounds.h};
        if (placements.size() > 1) {
            placements[1].bounds = {bounds.x + half, bounds.y, half, bounds.h};
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::ContextMenu) return ui::EventResult::Ignored;
        ++state_->context_events;
        return state_->result;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<MenuStageState> state_;
};

class MenuStage {
public:
    MenuStage(std::shared_ptr<MenuStageState> state, MenuProbe left, MenuProbe right)
        : state_(std::move(state)) {
        children_.push_back(std::move(left).spec());
        children_.push_back(std::move(right).spec());
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<MenuStageComponent>(state); },
            std::move(children_)};
    }

private:
    std::shared_ptr<MenuStageState> state_;
    std::vector<ui::Spec> children_;
};

void context_menu_delivers_to_the_hit_target() {
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    // Activation focuses the first focusable child.
    NUI_CHECK(left->focus_in == 1);
    NUI_CHECK(right->focus_in == 0);

    auto event = test::pointer(ui::InputType::ContextMenu, 180.0f, 40.0f);
    event.shift = true;
    event.primary = true;
    right->result = ui::EventResult::Handled;
    right->capture_on_context = true;
    const int capture_before = platform.pointer_capture_begin_count;

    NUI_CHECK(tree.dispatch(event, platform) == ui::EventResult::Handled);
    NUI_CHECK(right->context_events == 1);
    NUI_CHECK(stage->context_events == 0);
    NUI_CHECK_NEAR(right->last_position.x, 180.0f, 0.01f);
    NUI_CHECK_NEAR(right->last_position.y, 40.0f, 0.01f);
    NUI_CHECK(right->shift);
    NUI_CHECK(right->primary);

    // No focus change, no new capture (even when the handler asks for one),
    // and no primary-press semantics.
    NUI_CHECK(left->focus_in == 1);
    NUI_CHECK(left->focus_out == 0);
    NUI_CHECK(right->focus_in == 0);
    NUI_CHECK(right->focus_out == 0);
    NUI_CHECK(platform.pointer_capture_begin_count == capture_before);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
    NUI_CHECK(left->pointer_events == 0);
    NUI_CHECK(right->pointer_events == 0);
}

void context_menu_bubbles_and_keeps_press_behavior() {
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    // Ignored menu events bubble to the ancestor; a descendant handler stops
    // them before the ancestor.
    stage->result = ui::EventResult::Handled;
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(left->context_events == 1);
    NUI_CHECK(stage->context_events == 1);
    left->result = ui::EventResult::Handled;
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(left->context_events == 2);
    NUI_CHECK(stage->context_events == 1);

    // Outside every target the request is ignored and reaches nobody.
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 900.0f, 900.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(left->context_events == 2);

    // Left-button presses keep their semantics: delivery and focus targeting.
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 180.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(right->pointer_events == 1);
    NUI_CHECK(right->focus_in == 1);
    NUI_CHECK(left->focus_out == 1);
    NUI_CHECK(right->context_events == 0);

    // A menu request after the press does not move focus again. The right probe
    // ignores it, so the ancestor's Handled result is what the caller sees.
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 180.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(right->context_events == 1);
    NUI_CHECK(stage->context_events == 2);
    NUI_CHECK(right->focus_in == 1);
    NUI_CHECK(left->focus_out == 1);
}

void context_menu_does_not_count_as_a_click() {
    // Regression: a menu request must not feed the press path, so a PointerDown
    // immediately after it still behaves like a fresh single press.
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    auto press = test::pointer(ui::InputType::PointerDown, 60.0f, 40.0f);
    press.clicks = 1;
    NUI_CHECK(tree.dispatch(press, platform) == ui::EventResult::Ignored);
    NUI_CHECK(left->pointer_events == 1);
    NUI_CHECK(left->focus_in == 1);
}

void context_menu_releases_an_active_capture() {
    // A menu request must never coexist with a stale drag: the capture owner
    // receives PointerCancel, the capture is released, and the request goes to
    // the pointer hit target.
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    right->capture_on_press = true;
    right->result = ui::EventResult::Handled;
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 180.0f, 40.0f), platform);
    NUI_CHECK(right->pointer_events == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 0);

    left->result = ui::EventResult::Handled;
    left->capture_on_context = true;
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(right->cancel_events == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    NUI_CHECK(left->context_events == 1);
    NUI_CHECK(right->context_events == 0);

    // The temporary no-capture guard is restored after normal delivery.
    left->capture_on_context = false;
    left->capture_on_press = true;
    (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_begin_count == 2);
    (void)tree.dispatch(test::pointer(ui::InputType::PointerCancel, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_end_count == 2);
}

void context_menu_recovers_when_capture_cancel_throws() {
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    right->capture_on_press = true;
    right->throw_on_cancel = true;
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 180.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 0);

    bool threw = false;
    try {
        (void)tree.dispatch(
            test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(right->cancel_events == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    NUI_CHECK(left->context_events == 0);

    // The failed cancellation still leaves the tree in a coherent idle state:
    // capture is gone, the no-capture guard is restored, and a fresh pointer
    // interaction can capture/release normally.
    right->throw_on_cancel = false;
    left->capture_on_press = true;
    (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_begin_count == 2);
    (void)tree.dispatch(test::pointer(ui::InputType::PointerCancel, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_end_count == 2);
}

void context_menu_capture_guard_recovers_after_throw() {
    auto left = std::make_shared<MenuProbeState>();
    auto right = std::make_shared<MenuProbeState>();
    auto stage = std::make_shared<MenuStageState>();
    ui::UI tree{MenuStage{stage, MenuProbe{left}, MenuProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    left->capture_on_context = true;
    left->throw_on_context = true;
    bool threw = false;
    try {
        (void)tree.dispatch(
            test::pointer(ui::InputType::ContextMenu, 60.0f, 40.0f), platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(platform.pointer_capture_begin_count == 0);
    NUI_CHECK(platform.pointer_capture_end_count == 0);

    // Exception recovery must restore the guard so later ordinary pointer input
    // can capture and release normally.
    left->capture_on_context = false;
    left->throw_on_context = false;
    left->capture_on_press = true;
    (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    (void)tree.dispatch(test::pointer(ui::InputType::PointerCancel, 60.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
}

} // namespace

int main() {
    return test::run("t175 context menu input", [] {
        context_menu_delivers_to_the_hit_target();
        context_menu_bubbles_and_keeps_press_behavior();
        context_menu_does_not_count_as_a_click();
        context_menu_releases_an_active_capture();
        context_menu_recovers_when_capture_cancel_throws();
        context_menu_capture_guard_recovers_after_throw();
    });
}
