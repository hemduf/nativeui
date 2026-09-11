#include "example_support.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace {

struct DemoState {
    ui::UI* ui{};
    ui::OverlayHandle popup;
    ui::OverlayHandle modal;
    ui::OverlayHandle tooltip;
};

struct CaptureState {
    int pointer_downs{};
    int pointer_moves{};
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
        if (event.type == ui::InputType::PointerCancel) {
            ++state_->pointer_cancels;
            state_->cancelled_reentrantly =
                state_->cancelled_reentrantly || state_->in_pointer_down;
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerMove) {
            ++state_->pointer_moves;
            return ui::EventResult::Handled;
        }
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;

        ++state_->pointer_downs;
        state_->in_pointer_down = true;
        context.capture_pointer();
        if (state_->show_modal) state_->modal = state_->show_modal();
        state_->in_pointer_down = false;
        return ui::EventResult::Handled;
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

ui::OverlaySpec centered_label(std::string text) {
    ui::OverlaySpec overlay;
    overlay.placement = ui::OverlayPlacement::Center;
    overlay.content = ui::make_spec(
        ui::Padding{16.0f, ui::Label{std::move(text)}});
    return overlay;
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T061 — overlay portal"},
            ui::Label{
                "Overlays stay inside the owning UI and share the T058 structural checkpoint."}
                .size(13.0f),
            ui::Row{
                ui::Button{"Show popup", [&state] {
                    if (!state.ui || state.popup.valid()) return;
                    auto overlay = centered_label("Non-modal popup — click outside to dismiss");
                    overlay.dismiss_on_escape = true;
                    overlay.dismiss_on_outside_pointer_down = true;
                    state.popup = state.ui->show_overlay(std::move(overlay));
                }},
                ui::Button{"Show modal", [&state] {
                    if (!state.ui || state.modal.valid()) return;
                    auto overlay = centered_label("Modal overlay — Escape closes it");
                    overlay.mode = ui::OverlayMode::Modal;
                    overlay.dismiss_on_escape = true;
                    state.modal = state.ui->show_overlay(std::move(overlay));
                }},
                ui::Button{"Show pointer-transparent", [&state] {
                    if (!state.ui || state.tooltip.valid()) return;
                    auto overlay = centered_label("Pointer-transparent overlay");
                    overlay.pointer_policy = ui::OverlayPointerPolicy::Ignore;
                    overlay.dismiss_on_escape = true;
                    state.tooltip = state.ui->show_overlay(std::move(overlay));
                }},
                ui::Button{"Close all", [&state] {
                    if (!state.ui) return;
                    (void)state.ui->close_overlay(state.modal);
                    (void)state.ui->close_overlay(state.popup);
                    (void)state.ui->close_overlay(state.tooltip);
                }}
            }.gap(10.0f),
            ui::Label{
                "Later overlays are topmost. Modal overlays block lower input; pointer-transparent overlays do not."
            }.size(12.0f).color(ui::colors::textMuted)
        }.gap(12.0f)
    };
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    state.ui = &tree;
    ui::HeadlessRenderer renderer{{760.0f, 280.0f}, 1.0f};

    if (!renderer.render(tree)) return example::fail("initial overlay render failed");

    auto popup = centered_label("Popup");
    popup.dismiss_on_outside_pointer_down = true;
    state.popup = tree.show_overlay(std::move(popup));
    if (!state.popup.valid() || !renderer.render(tree)) {
        return example::fail("non-modal overlay failed");
    }

    auto modal = centered_label("Modal");
    modal.mode = ui::OverlayMode::Modal;
    modal.dismiss_on_escape = true;
    state.modal = tree.show_overlay(std::move(modal));
    if (!state.modal.valid() || !renderer.render(tree)) {
        return example::fail("modal overlay failed");
    }

    example::Platform platform;
    if (!ui::handled(tree.dispatch(example::key(ui::Key::Escape), platform)) ||
        state.modal.valid()) {
        return example::fail("modal Escape dismissal failed");
    }

    auto transparent = centered_label("Transparent");
    transparent.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    transparent.dismiss_on_escape = true;
    state.tooltip = tree.show_overlay(std::move(transparent));
    if (!state.tooltip.valid() || !renderer.render(tree)) {
        return example::fail("pointer-transparent overlay failed");
    }
    if (!ui::handled(tree.dispatch(example::key(ui::Key::Escape), platform)) ||
        state.tooltip.valid()) {
        return example::fail("pointer-transparent Escape dismissal failed");
    }

    if (!tree.close_overlay(state.popup)) {
        return example::fail("overlay close failed");
    }
    if (!renderer.render(tree)) return example::fail("final overlay teardown render failed");
    if (state.popup.valid()) {
        return example::fail("closed overlay handle remained valid");
    }

    // A normal non-modal overlay must not steal keyboard focus from its root.
    // A modal with no focusable content must still own keyboard activation and
    // restore the root when it closes.
    int a_activations = 0;
    int b_activations = 0;
    ui::UI a_ui{ui::Button{"A", [&] { ++a_activations; }}};
    ui::UI b_ui{ui::Button{"B", [&] { ++b_activations; }}};
    a_ui.resize({96.0f, 48.0f});
    b_ui.resize({96.0f, 48.0f});
    a_ui.activate(platform);
    b_ui.activate(platform);

    auto non_modal = centered_label("Non-modal keyboard probe");
    const auto non_modal_handle = a_ui.show_overlay(std::move(non_modal));
    a_ui.resize({96.0f, 48.0f});
    if (!non_modal_handle.valid() ||
        !ui::handled(a_ui.dispatch(example::key(ui::Key::Enter), platform)) ||
        a_activations != 1) {
        return example::fail("non-modal overlay stole root keyboard focus");
    }
    if (!a_ui.close_overlay(non_modal_handle)) {
        return example::fail("non-modal keyboard probe close failed");
    }
    a_ui.resize({96.0f, 48.0f});

    auto key_modal = centered_label("Modal keyboard probe");
    key_modal.mode = ui::OverlayMode::Modal;
    const auto key_modal_handle = a_ui.show_overlay(std::move(key_modal));
    a_ui.resize({96.0f, 48.0f});
    if (!key_modal_handle.valid() ||
        !ui::handled(a_ui.dispatch(example::key(ui::Key::Enter), platform)) ||
        a_activations != 1) {
        return example::fail("modal overlay allowed lower keyboard activation");
    }

    // A later NonModal overlay may receive pointer input above the modal, but
    // that pointer interaction must not move keyboard focus out of the active
    // modal trap. Otherwise Tab can wrap into root content under the modal.
    int above_modal_activations = 0;
    ui::OverlaySpec above_modal;
    above_modal.placement = ui::OverlayPlacement::Center;
    above_modal.content = ui::make_spec(
        ui::Button{"Above modal", [&] { ++above_modal_activations; }});
    const auto above_modal_handle = a_ui.show_overlay(std::move(above_modal));
    a_ui.resize({96.0f, 48.0f});

    ui::InputEvent above_down;
    above_down.type = ui::InputType::PointerDown;
    above_down.position = {48.0f, 24.0f};
    ui::InputEvent above_up = above_down;
    above_up.type = ui::InputType::PointerUp;
    if (!above_modal_handle.valid() ||
        !ui::handled(a_ui.dispatch(above_down, platform)) ||
        !ui::handled(a_ui.dispatch(above_up, platform)) ||
        above_modal_activations != 1) {
        return example::fail("above-modal non-modal pointer input failed");
    }
    (void)a_ui.dispatch(example::key(ui::Key::Tab), platform);
    (void)a_ui.dispatch(example::key(ui::Key::Enter), platform);
    if (a_activations != 1) {
        return example::fail("above-modal pointer focus escaped modal trap");
    }
    if (!a_ui.close_overlay(above_modal_handle)) {
        return example::fail("above-modal non-modal close failed");
    }
    a_ui.resize({96.0f, 48.0f});

    // The modal/focus state of A must not perturb an independent UI B.
    if (!ui::handled(b_ui.dispatch(example::key(ui::Key::Enter), platform)) ||
        b_activations != 1) {
        return example::fail("modal focus leaked across UI instances");
    }

    if (!a_ui.close_overlay(key_modal_handle)) {
        return example::fail("modal keyboard probe close failed");
    }
    a_ui.resize({96.0f, 48.0f});
    if (!ui::handled(a_ui.dispatch(example::key(ui::Key::Enter), platform)) ||
        a_activations != 2 || b_activations != 1) {
        return example::fail("modal focus restoration or UI isolation failed");
    }

    // A normal non-modal overlay only owns its own region. Pointer input outside
    // it remains eligible for the underlying root when no outside-dismiss policy
    // consumes the event. Use fixed-size content so the test point is guaranteed
    // to remain outside regardless of platform text metrics.
    auto outside_state = std::make_shared<CaptureState>();
    ui::UI outside_tree{CaptureProbe{outside_state}};
    outside_tree.resize({96.0f, 48.0f});
    outside_tree.activate(platform);
    ui::OverlaySpec outside_overlay;
    outside_overlay.placement = ui::OverlayPlacement::Center;
    outside_overlay.content = ui::make_spec(ui::Spacer{24.0f, 16.0f});
    const auto outside_handle = outside_tree.show_overlay(std::move(outside_overlay));
    outside_tree.resize({96.0f, 48.0f});
    ui::InputEvent outside_down;
    outside_down.type = ui::InputType::PointerDown;
    outside_down.position = {2.0f, 2.0f};
    if (!outside_handle.valid() ||
        !ui::handled(outside_tree.dispatch(outside_down, platform)) ||
        outside_state->pointer_downs != 1 || !outside_handle.valid()) {
        return example::fail("non-modal outside pointer did not reach root");
    }
    if (!outside_tree.close_overlay(outside_handle)) {
        return example::fail("non-modal outside probe close failed");
    }

    auto capture = std::make_shared<CaptureState>();
    ui::UI capture_tree{CaptureProbe{capture}};
    capture_tree.resize({96.0f, 48.0f});
    capture_tree.activate(platform);
    capture->show_modal = [&capture_tree] {
        auto capture_modal = centered_label("Capture barrier");
        capture_modal.mode = ui::OverlayMode::Modal;
        return capture_tree.show_overlay(std::move(capture_modal));
    };

    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {4.0f, 4.0f};
    if (!ui::handled(capture_tree.dispatch(down, platform)) ||
        capture->pointer_downs != 1 || !capture->modal.valid()) {
        return example::fail("modal capture setup failed");
    }
    if (capture->pointer_cancels != 1 || capture->cancelled_reentrantly) {
        return example::fail("modal did not safely cancel lower pointer capture");
    }

    ui::InputEvent move;
    move.type = ui::InputType::PointerMove;
    move.position = {4.0f, 4.0f};
    if (!ui::handled(capture_tree.dispatch(move, platform)) ||
        capture->pointer_moves != 0 || capture->pointer_cancels != 1) {
        return example::fail("lower pointer capture bypassed modal barrier");
    }
    if (!capture_tree.close_overlay(capture->modal)) {
        return example::fail("modal capture barrier close failed");
    }

    outside_tree.deactivate(platform);
    a_ui.deactivate(platform);
    b_ui.deactivate(platform);
    capture_tree.deactivate(platform);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    state.ui = &tree;
    return example::run_window(tree, "NativeUI T061 Overlay Portal", {760.0f, 280.0f});
}
