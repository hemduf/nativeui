#include "test_support.hpp"

#include <memory>

namespace {

struct CaptureState {
    int down{};
    int move{};
    int up{};
    int cancel{};
    bool release_on_up{};
    bool recapture_on_cancel{};
};

class CaptureProbeComponent final : public ui::Component {
public:
    explicit CaptureProbeComponent(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 60.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++state_->down;
            context.capture_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            ++state_->up;
            if (state_->release_on_up) context.release_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++state_->cancel;
            if (state_->recapture_on_cancel) context.capture_pointer();
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
    explicit CaptureProbe(std::shared_ptr<CaptureState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state] { return std::make_unique<CaptureProbeComponent>(state); }, {}};
    }
private:
    std::shared_ptr<CaptureState> state_;
};

void suite() {
    test::MockPlatform platform;

    // Captured motion remains authoritative outside bounds and PointerUp always
    // terminates capture even when the component forgets to release explicitly.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->move == 1);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->up == 1);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(state->move == 1);
    }

    // Explicit component release and tree auto-release are idempotent.
    {
        auto state = std::make_shared<CaptureState>();
        state->release_on_up = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 0);
    }

    // Deactivate/focus-loss delivers exactly one cancellation while the tree is
    // still active, then repeated lifecycle calls cannot cancel a stale owner.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);

        tree.deactivate(platform);
        NUI_CHECK(state->cancel == 1);
        tree.deactivate(platform);
        NUI_CHECK(state->cancel == 1);

        tree.activate(platform);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(state->move == 0);
    }

    // A second PointerDown cancels a missing-up interaction before beginning a
    // new one, preventing ownership from silently leaking across gestures.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(state->cancel == 1);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
    }

    // Cancellation handlers cannot accidentally resurrect capture. This also
    // guarantees a host-close/focus-loss cancellation is terminal.
    {
        auto state = std::make_shared<CaptureState>();
        state->recapture_on_cancel = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 1);
    }
}

} // namespace

int main() { return test::run("pointer_capture", &suite); }
