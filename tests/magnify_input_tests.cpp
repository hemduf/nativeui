#include "test_support.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {

// InputType::Magnify (trackpad pinch / normalized ctrl/cmd zoom) is delivered
// like PointerWheel: to the pointer hit target with its position, factor and
// modifiers preserved, without moving keyboard focus, without establishing a
// capture and without disturbing an active drag (the capture owner receives
// it like a wheel event during the drag).

struct ZoomProbeState {
    int magnify_events{};
    int wheel_events{};
    int pointer_events{};
    int cancel_events{};
    int focus_in{};
    int focus_out{};
    ui::Point last_position{};
    float last_magnification{};
    bool last_ctrl{};
    bool capture_on_press{};
    ui::EventResult magnify_result{ui::EventResult::Ignored};
    ui::EventResult wheel_result{ui::EventResult::Ignored};
};

class ZoomProbeComponent final : public ui::Component {
public:
    explicit ZoomProbeComponent(std::shared_ptr<ZoomProbeState> state)
        : state_(std::move(state)) {}

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
        if (event.type == ui::InputType::PointerCancel) ++state_->cancel_events;
        if (event.type == ui::InputType::Magnify) {
            ++state_->magnify_events;
            state_->last_position = event.position;
            state_->last_magnification = event.magnification;
            state_->last_ctrl = event.ctrl;
            return state_->magnify_result;
        }
        if (event.type == ui::InputType::PointerWheel) {
            ++state_->wheel_events;
            return state_->wheel_result;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ZoomProbeState> state_;
};

class ZoomProbe {
public:
    explicit ZoomProbe(std::shared_ptr<ZoomProbeState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<ZoomProbeComponent>(state); },
            {}};
    }

private:
    std::shared_ptr<ZoomProbeState> state_;
};

struct ZoomStageState {
    int magnify_events{};
    ui::EventResult magnify_result{ui::EventResult::Ignored};
};

// Lays out its two children as equal columns so hit targeting can be checked
// independently for each half.
class ZoomStageComponent final : public ui::Component {
public:
    explicit ZoomStageComponent(std::shared_ptr<ZoomStageState> state)
        : state_(std::move(state)) {}

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
        if (event.type != ui::InputType::Magnify) return ui::EventResult::Ignored;
        ++state_->magnify_events;
        return state_->magnify_result;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ZoomStageState> state_;
};

class ZoomStage {
public:
    ZoomStage(std::shared_ptr<ZoomStageState> state, ZoomProbe left, ZoomProbe right)
        : state_(std::move(state)) {
        children_.push_back(std::move(left).spec());
        children_.push_back(std::move(right).spec());
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<ZoomStageComponent>(state); },
            std::move(children_)};
    }

private:
    std::shared_ptr<ZoomStageState> state_;
    std::vector<ui::Spec> children_;
};

ui::InputEvent magnify(float x, float y, float factor) {
    ui::InputEvent event{};
    event.type = ui::InputType::Magnify;
    event.position = {x, y};
    event.magnification = factor;
    event.ctrl = true;
    return event;
}

void magnify_delivers_to_the_pointer_hit_target() {
    auto left = std::make_shared<ZoomProbeState>();
    auto right = std::make_shared<ZoomProbeState>();
    auto stage = std::make_shared<ZoomStageState>();
    ui::UI tree{ZoomStage{stage, ZoomProbe{left}, ZoomProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    // Activation focuses the first focusable child.
    NUI_CHECK(left->focus_in == 1);

    right->magnify_result = ui::EventResult::Handled;
    NUI_CHECK(tree.dispatch(magnify(180.0f, 40.0f, 0.04f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(right->magnify_events == 1);
    NUI_CHECK(stage->magnify_events == 0);
    NUI_CHECK(left->magnify_events == 0);
    NUI_CHECK_NEAR(right->last_position.x, 180.0f, 0.01f);
    NUI_CHECK_NEAR(right->last_position.y, 40.0f, 0.01f);
    NUI_CHECK_NEAR(right->last_magnification, 0.04f, 1e-6f);
    NUI_CHECK(right->last_ctrl);

    // No focus change and no capture bookkeeping.
    NUI_CHECK(left->focus_in == 1);
    NUI_CHECK(left->focus_out == 0);
    NUI_CHECK(right->focus_in == 0);
    NUI_CHECK(right->focus_out == 0);
    NUI_CHECK(platform.pointer_capture_begin_count == 0);
    NUI_CHECK(platform.pointer_capture_end_count == 0);
    NUI_CHECK(left->pointer_events == 0);
    NUI_CHECK(right->pointer_events == 0);
}

void magnify_bubbles_and_keeps_wheel_side_channels() {
    auto left = std::make_shared<ZoomProbeState>();
    auto right = std::make_shared<ZoomProbeState>();
    auto stage = std::make_shared<ZoomStageState>();
    ui::UI tree{ZoomStage{stage, ZoomProbe{left}, ZoomProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    // Ignored magnify events bubble to the ancestor like other targeted input.
    stage->magnify_result = ui::EventResult::Handled;
    NUI_CHECK(tree.dispatch(magnify(60.0f, 40.0f, -0.02f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(left->magnify_events == 1);
    NUI_CHECK(stage->magnify_events == 1);

    // Outside every target the event is ignored and reaches nobody.
    NUI_CHECK(tree.dispatch(magnify(900.0f, 900.0f, 0.5f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(left->magnify_events == 1);

    // Plain scroll (no ctrl/gui) keeps arriving as PointerWheel.
    auto wheel = test::pointer(ui::InputType::PointerWheel, 60.0f, 40.0f);
    wheel.delta = {0.0f, 1.0f};
    NUI_CHECK(tree.dispatch(wheel, platform) == ui::EventResult::Ignored);
    NUI_CHECK(right->wheel_events == 0);
    NUI_CHECK(left->wheel_events == 1);
    NUI_CHECK(stage->magnify_events == 1);
}

void magnify_reaches_the_capture_owner_during_a_drag() {
    // Wheel parity: while a drag owns the pointer, a pinch event routes to the
    // capture owner instead of retargeting, and the drag state survives.
    auto left = std::make_shared<ZoomProbeState>();
    auto right = std::make_shared<ZoomProbeState>();
    right->capture_on_press = true;
    right->magnify_result = ui::EventResult::Handled;
    auto stage = std::make_shared<ZoomStageState>();
    ui::UI tree{ZoomStage{stage, ZoomProbe{left}, ZoomProbe{right}}};
    test::MockPlatform platform;
    tree.resize({240.0f, 80.0f});
    tree.activate(platform);

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 180.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(right->pointer_events == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 0);

    NUI_CHECK(tree.dispatch(magnify(60.0f, 40.0f, 0.03f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(right->magnify_events == 1);
    NUI_CHECK(left->magnify_events == 0);
    NUI_CHECK(stage->magnify_events == 0);
    NUI_CHECK(right->cancel_events == 0);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 0);

    // The drag can end normally afterwards.
    (void)tree.dispatch(test::pointer(ui::InputType::PointerCancel, 180.0f, 40.0f), platform);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
}

} // namespace

int main() {
    return test::run("magnify input routing", [] {
        magnify_delivers_to_the_pointer_hit_target();
        magnify_bubbles_and_keeps_wheel_side_channels();
        magnify_reaches_the_capture_owner_during_a_drag();
    });
}
