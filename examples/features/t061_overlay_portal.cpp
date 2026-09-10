#include "example_support.hpp"

namespace {

struct DemoState {
    ui::UI* ui{};
    ui::OverlayHandle popup;
    ui::OverlayHandle modal;
    ui::OverlayHandle tooltip;
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
    state.tooltip = tree.show_overlay(std::move(transparent));
    if (!state.tooltip.valid() || !renderer.render(tree)) {
        return example::fail("pointer-transparent overlay failed");
    }

    if (!tree.close_overlay(state.popup) || !tree.close_overlay(state.tooltip)) {
        return example::fail("overlay close failed");
    }
    if (!renderer.render(tree)) return example::fail("final overlay teardown render failed");
    if (state.popup.valid() || state.tooltip.valid()) {
        return example::fail("closed overlay handle remained valid");
    }

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
