#include "test_support.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace {

struct CaptureState {
    int pointer_downs{};
    int pointer_moves{};
    int pointer_ups{};
    int pointer_cancels{};
    bool in_pointer_down{};
    bool cancelled_reentrantly{};
    ui::OverlayHandle modal;
    std::function<ui::OverlayHandle()> show_modal;
};

class CaptureComponent final : public ui::Component {
public:
    explicit CaptureComponent(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
            case ui::InputType::PointerDown:
                ++state_->pointer_downs;
                state_->in_pointer_down = true;
                context.capture_pointer();
                if (state_->show_modal) state_->modal = state_->show_modal();
                state_->in_pointer_down = false;
                return ui::EventResult::Handled;
            case ui::InputType::PointerMove:
                ++state_->pointer_moves;
                return ui::EventResult::Handled;
            case ui::InputType::PointerUp:
                ++state_->pointer_ups;
                return ui::EventResult::Handled;
            case ui::InputType::PointerCancel:
                ++state_->pointer_cancels;
                state_->cancelled_reentrantly =
                    state_->cancelled_reentrantly || state_->in_pointer_down;
                return ui::EventResult::Handled;
            default:
                return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<CaptureState> state_;
};

class CaptureProbe {
public:
    explicit CaptureProbe(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<CaptureComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<CaptureState> state_;
};

ui::OverlaySpec modal_overlay() {
    ui::OverlaySpec modal;
    modal.mode = ui::OverlayMode::Modal;
    modal.placement = ui::OverlayPlacement::Center;
    modal.content = ui::make_spec(ui::Spacer{24.0f, 16.0f});
    return modal;
}

ui::InputEvent pointer(ui::InputType type, ui::Point position) {
    ui::InputEvent event;
    event.type = type;
    event.position = position;
    return event;
}

void programmatic_modal_cancels_lower_capture() {
    test::MockPlatform platform;
    auto state = std::make_shared<CaptureState>();
    ui::UI tree{CaptureProbe{state}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(
        pointer(ui::InputType::PointerDown, {4.0f, 4.0f}), platform)));
    NUI_CHECK(state->pointer_downs == 1);
    NUI_CHECK(state->pointer_cancels == 0);

    const auto modal = tree.show_overlay(modal_overlay());
    NUI_CHECK(modal.valid());
    tree.resize({96.0f, 48.0f});

    NUI_CHECK(ui::handled(tree.dispatch(
        pointer(ui::InputType::PointerMove, {4.0f, 4.0f}), platform)));
    NUI_CHECK(state->pointer_cancels == 1);
    NUI_CHECK(state->pointer_moves == 0);

    NUI_CHECK(ui::handled(tree.dispatch(
        pointer(ui::InputType::PointerUp, {4.0f, 4.0f}), platform)));
    NUI_CHECK(state->pointer_cancels == 1);
    NUI_CHECK(state->pointer_ups == 0);
    NUI_CHECK(tree.close_overlay(modal));
}

void reentrant_modal_show_cancels_after_callback_unwinds() {
    test::MockPlatform platform;
    auto state = std::make_shared<CaptureState>();
    ui::UI tree{CaptureProbe{state}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    state->show_modal = [&tree] { return tree.show_overlay(modal_overlay()); };

    NUI_CHECK(ui::handled(tree.dispatch(
        pointer(ui::InputType::PointerDown, {4.0f, 4.0f}), platform)));
    NUI_CHECK(state->pointer_downs == 1);
    NUI_CHECK(state->modal.valid());
    NUI_CHECK(state->pointer_cancels == 1);
    NUI_CHECK(!state->cancelled_reentrantly);

    NUI_CHECK(ui::handled(tree.dispatch(
        pointer(ui::InputType::PointerMove, {4.0f, 4.0f}), platform)));
    NUI_CHECK(state->pointer_moves == 0);
    NUI_CHECK(state->pointer_cancels == 1);
    NUI_CHECK(tree.close_overlay(state->modal));
}

void suite() {
    programmatic_modal_cancels_lower_capture();
    reentrant_modal_show_cancels_after_callback_unwinds();
}

} // namespace

int main() { return test::run("t061_modal_capture_barrier", &suite); }
