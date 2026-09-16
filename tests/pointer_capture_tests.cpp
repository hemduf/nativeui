#include "test_support.hpp"

#include <functional>
#include <memory>
#include <stdexcept>

namespace ui {
struct TreeTestAccess {
    [[nodiscard]] static auto dispatch_depth(const Tree& tree) noexcept {
        return tree.dispatch_depth_;
    }
    [[nodiscard]] static bool pointer_interaction_active(const Tree& tree) noexcept {
        return tree.pointer_interaction_active_;
    }
};
} // namespace ui

namespace {

struct CaptureState {
    int down{};
    int move{};
    int up{};
    int cancel{};
    bool release_on_up{};
    bool recapture_on_cancel{};
    bool throw_on_down{};
    bool throw_on_up{};
    bool throw_on_cancel{};
    std::function<void(ui::InputContext&)> on_cancel;
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
            if (state_->throw_on_down) {
                state_->throw_on_down = false;
                throw std::runtime_error("pointer down fault");
            }
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            ++state_->up;
            if (state_->release_on_up) context.release_pointer();
            if (state_->throw_on_up) {
                state_->throw_on_up = false;
                throw std::runtime_error("pointer up fault");
            }
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++state_->cancel;
            if (state_->recapture_on_cancel) context.capture_pointer();
            if (state_->on_cancel) state_->on_cancel(context);
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

struct HoverRecoveryState {
    int enter{};
    int move{};
};

class HoverRecoveryComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit HoverRecoveryComponent(std::shared_ptr<HoverRecoveryState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 60.0f};
    }
    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerMove) {
            ++state_->move;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void retained_pointer_hover_changed(bool hovered, bool, ui::Dispatcher) override {
        if (!hovered) return;
        ++state_->enter;
        throw std::runtime_error("persistent hover observer failure");
    }

private:
    std::shared_ptr<HoverRecoveryState> state_;
};

class HoverRecoveryProbe {
public:
    explicit HoverRecoveryProbe(std::shared_ptr<HoverRecoveryState> state)
        : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state] { return std::make_unique<HoverRecoveryComponent>(state); }, {}};
    }
private:
    std::shared_ptr<HoverRecoveryState> state_;
};

struct PostRouteCaptureState {
    int down{};
    int move{};
    int cancel{};
};

class PostRouteCaptureComponent final : public ui::Component {
public:
    PostRouteCaptureComponent(
        std::shared_ptr<PostRouteCaptureState> state,
        ui::State<bool>& reveal)
        : state_(std::move(state)), reveal_(&reveal) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 60.0f};
    }
    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++state_->down;
            context.capture_pointer();
            reveal_->set(true);
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++state_->cancel;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

private:
    std::shared_ptr<PostRouteCaptureState> state_;
    ui::State<bool>* reveal_{};
};

class PostRouteCaptureProbe {
public:
    PostRouteCaptureProbe(
        std::shared_ptr<PostRouteCaptureState> state,
        ui::State<bool>& reveal)
        : state_(std::move(state)), reveal_(&reveal) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        auto* reveal = reveal_;
        return ui::Spec{
            [state, reveal] {
                return std::make_unique<PostRouteCaptureComponent>(state, *reveal);
            },
            {}};
    }

private:
    std::shared_ptr<PostRouteCaptureState> state_;
    ui::State<bool>* reveal_{};
};

class ThrowOnceDynamicLeafComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {10.0f, 10.0f};
    }
    void paint(ui::PaintContext&) const override {}
};

class ThrowOnceDynamicLeaf {
public:
    explicit ThrowOnceDynamicLeaf(std::shared_ptr<int> attempts)
        : attempts_(std::move(attempts)) {}

    ui::Spec spec() && {
        auto attempts = std::move(attempts_);
        return ui::Spec{
            [attempts] {
                ++*attempts;
                if (*attempts == 1) {
                    throw std::runtime_error("finish reconciliation failure");
                }
                return std::make_unique<ThrowOnceDynamicLeafComponent>();
            },
            {}};
    }

private:
    std::shared_ptr<int> attempts_;
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

    // T125: PointerDown starts interaction bookkeeping before it cancels a stale
    // capture. If that cancellation callback throws, the failed PointerDown has
    // not completed and must not leave retained interaction state permanently
    // active. Cleanup is framework-only; a later gesture can capture normally.
    {
        auto state = std::make_shared<CaptureState>();
        ui::Tree tree{ui::compile(ui::make_spec(CaptureProbe{state}))};
        tree.mount();
        tree.layout({120.0f, 80.0f});
        tree.activate_focus(platform);

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);

        state->throw_on_cancel = true;
        bool threw = false;
        try {
            (void)tree.dispatch(
                test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(!ui::TreeTestAccess::pointer_interaction_active(tree));
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

        tree.dispatch(test::pointer(ui::InputType::PointerDown, 40.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 40.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 2);
    }

    // T125: PointerDown may acquire retained/native capture before component
    // code throws. Unwind cleanup must terminate that capture without issuing a
    // synthetic PointerCancel, and a later gesture must be able to capture again.
    {
        auto state = std::make_shared<CaptureState>();
        state->throw_on_down = true;
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        bool threw = false;
        try {
            (void)tree.dispatch(
                test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->down == 1);
        NUI_CHECK(state->cancel == 0);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 0);

        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 2);
    }

    // T125: a throwing PointerUp must not retain capture merely because normal
    // dispatch cleanup was skipped. The original exception is preserved, no
    // cancellation callback is synthesized during unwind, and the next gesture
    // observes an ordinary acquire/release lifetime.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);

        state->throw_on_up = true;
        bool threw = false;
        try {
            (void)tree.dispatch(
                test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->up == 1);
        NUI_CHECK(state->cancel == 0);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(state->cancel == 0);

        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(state->down == 2);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(state->up == 2);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 2);
    }

    // T125: nested cancellation must restore the previous guard value rather
    // than forcing false. After the inner cancel returns, the outer callback's
    // recapture attempt must still be suppressed; once the outer frame exits,
    // later ordinary capture must work again.
    {
        auto state = std::make_shared<CaptureState>();
        ui::UI tree{CaptureProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        bool reenter_once = true;
        state->on_cancel = [&](ui::InputContext& context) {
            if (!reenter_once) return;
            reenter_once = false;
            NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
            context.capture_pointer();
        };

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
        NUI_CHECK(state->cancel == 2);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

        state->on_cancel = {};
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 2);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 20.0f), platform);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 2);
    }

    // T125: direct global-command failure restores dispatch depth to the exact
    // pre-call level, and a later command uses an ordinary outermost frame.
    // A nested command failure caught by its outer callback must restore depth
    // back to one until the outer dispatch itself completes.
    {
        auto state = std::make_shared<CaptureState>();
        ui::Tree tree{ui::compile(ui::make_spec(CaptureProbe{state}))};
        tree.mount();
        tree.layout({120.0f, 80.0f});
        tree.activate_focus(platform);

        ui::InputEvent copy{};
        copy.type = ui::InputType::Command;
        copy.command = ui::Command::Copy;

        bool throw_once = true;
        int global_calls = 0;
        tree.set_global_command_handler([&](ui::Command) -> ui::EventResult {
            ++global_calls;
            if (throw_once) {
                throw_once = false;
                throw std::runtime_error("global command fault");
            }
            return ui::EventResult::Handled;
        });

        bool threw = false;
        try {
            (void)tree.dispatch(copy, platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(ui::TreeTestAccess::dispatch_depth(tree) == 0);
        NUI_CHECK(tree.dispatch(copy, platform) == ui::EventResult::Handled);
        NUI_CHECK(global_calls == 2);
        NUI_CHECK(ui::TreeTestAccess::dispatch_depth(tree) == 0);

        tree.set_global_command_handler([&](ui::Command command) -> ui::EventResult {
            if (command == ui::Command::Paste) {
                throw std::runtime_error("nested command fault");
            }
            if (command == ui::Command::Copy) {
                NUI_CHECK(ui::TreeTestAccess::dispatch_depth(tree) == 1);
                ui::InputEvent nested{};
                nested.type = ui::InputType::Command;
                nested.command = ui::Command::Paste;
                bool nested_threw = false;
                try {
                    (void)tree.dispatch(nested, platform);
                } catch (const std::runtime_error&) {
                    nested_threw = true;
                }
                NUI_CHECK(nested_threw);
                NUI_CHECK(ui::TreeTestAccess::dispatch_depth(tree) == 1);
                return ui::EventResult::Handled;
            }
            return ui::EventResult::Ignored;
        });

        NUI_CHECK(tree.dispatch(copy, platform) == ui::EventResult::Handled);
        NUI_CHECK(ui::TreeTestAccess::dispatch_depth(tree) == 0);
    }

    // T125 B2: publishing a new hover target before retained observer delivery
    // must not strand the transition when an observer throws. The same-target
    // follow-up resumes only the unstarted suffix; the persistent enter callback
    // is not retried and ordinary PointerMove routing remains usable.
    {
        auto state = std::make_shared<HoverRecoveryState>();
        ui::UI tree{HoverRecoveryProbe{state}};
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        bool threw = false;
        try {
            (void)tree.dispatch(
                test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state->enter == 1);
        NUI_CHECK(state->move == 0);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->enter == 1);
        NUI_CHECK(state->move == 1);
    }

    // T125 B2: once PointerDown target routing returns, capture and interaction
    // are semantically committed even if outermost finish reconciliation then
    // throws. Recovery of unrelated dynamic work must preserve that valid
    // capture until a later move/cancel terminates it normally.
    {
        ui::State<bool> reveal{false};
        auto state = std::make_shared<PostRouteCaptureState>();
        auto attempts = std::make_shared<int>(0);
        ui::Tree tree{ui::compile(ui::make_spec(
            ui::Column{
                PostRouteCaptureProbe{state, reveal},
                ui::If{reveal, ThrowOnceDynamicLeaf{attempts}}}
                .gap(4.0f)
                .padding(0.0f)))};
        tree.mount();
        tree.layout({120.0f, 100.0f});
        tree.activate_focus(platform);

        const int begin_before = platform.pointer_capture_begin_count;
        const int end_before = platform.pointer_capture_end_count;
        bool threw = false;
        try {
            (void)tree.dispatch(
                test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(*attempts == 1);
        NUI_CHECK(state->down == 1);
        NUI_CHECK(platform.pointer_capture_begin_count == begin_before + 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before);
        NUI_CHECK(ui::TreeTestAccess::pointer_interaction_active(tree));

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(*attempts == 2);
        NUI_CHECK(state->move == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerCancel, 500.0f, 500.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(state->cancel == 1);
        NUI_CHECK(platform.pointer_capture_end_count == end_before + 1);
        NUI_CHECK(!ui::TreeTestAccess::pointer_interaction_active(tree));
    }
}

} // namespace

int main() { return test::run("pointer_capture", &suite); }
