#include "test_support.hpp"

#include <memory>
#include <stdexcept>

namespace {

struct CaptureState {
    int down{};
    int move{};
    int up{};
    int cancel{};
    bool release_on_up{};
    bool recapture_on_cancel{};
    bool throw_on_cancel{};
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
            if (state_->throw_on_cancel) {
                state_->throw_on_cancel = false;
                throw std::runtime_error("pointer cancel fault");
            }
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
    // T044 additionally requires exactly one platform-boundary acquire/release
    // for the corresponding toolkit capture lifetime.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(platform.pointer_capture_begin_count == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 0);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->move == 1);
        NUI_CHECK(platform.pointer_capture_begin_count == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 0);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_begin_count == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 1);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(state->move == 1);
    }

    // Explicit component release and tree auto-release are idempotent at both
    // toolkit and platform boundaries.
    {
        auto state = std::make_shared<CaptureState>();
        state->release_on_up = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 2);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 0);
        NUI_CHECK(platform.pointer_capture_end_count == 2);
    }

    // Deactivate/focus-loss delivers exactly one cancellation while the tree is
    // still active, then repeated lifecycle calls cannot cancel a stale owner.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 3);

        tree.deactivate(platform);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 3);
        tree.deactivate(platform);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 3);

        tree.activate(platform);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(state->move == 0);
    }

    // A second PointerDown cancels a missing-up interaction before beginning a
    // new one. The platform capture lifetime follows the same end->begin handoff
    // and cannot silently leak across gestures.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 4);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_begin_count == 5);
        NUI_CHECK(platform.pointer_capture_end_count == 4);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 5);
    }

    // Cancellation handlers cannot accidentally resurrect capture. This also
    // guarantees a host-close/focus-loss cancellation is terminal at the
    // platform boundary.
    {
        auto state = std::make_shared<CaptureState>();
        state->recapture_on_cancel = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 6);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_end_count == 6);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_begin_count == 6);
        NUI_CHECK(platform.pointer_capture_end_count == 6);
    }

    // T125: a throwing PointerCancel cannot leave the cancellation guard or the
    // native capture owner wedged. The original exception remains observable to
    // the direct C++ caller, cleanup does not synthesize another callback, and a
    // later PointerDown can acquire/release a fresh capture normally.
    {
        auto state = std::make_shared<CaptureState>();
        state->throw_on_cancel = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);

        bool threw = false;
        try {
            (void)tree.cancel_pointer(platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 1);

        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 2);
    }
}

} // namespace

int main() { return test::run("pointer_capture", &suite); }