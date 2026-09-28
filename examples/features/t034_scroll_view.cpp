#include "example_support.hpp"

#include <cmath>
#include <string>

namespace {

struct DemoState {
    ui::ScrollState outer{ui::ScrollAxis::Vertical};
    ui::ScrollState inner{ui::ScrollAxis::Horizontal};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::ScrollView{
            state.outer,
            ui::Column{
                ui::Header{"T034 — ScrollView"},
                ui::Label{
                    "Outer vertical scrolling contains an independent horizontal ScrollView. "
                    "Wheel input bubbles at boundaries; both views expose persistent overlay "
                    "scrollbars and optional captured pointer panning."
                }.size(12.0f).color(ui::colors::textMuted),
                ui::ScrollView{
                    state.inner,
                    ui::Row{
                        example::Box{"A", {180.0f, 72.0f}},
                        example::Box{"B", {180.0f, 72.0f}, ui::colors::input},
                        example::Box{"C", {180.0f, 72.0f}}
                    }.gap(12.0f)}
                    .pointer_pan(true),
                ui::Spacer{520.0f, 260.0f},
                ui::Button{"Reveal lower content", [&state] {
                    (void)ui::ensure_visible(
                        state.outer,
                        {0.0f, 430.0f, 120.0f, 32.0f},
                        ui::ScrollAlignment::Nearest);
                }},
                ui::Spacer{520.0f, 260.0f}
            }.gap(12.0f).padding(12.0f)}
            .pointer_pan(true)};
}

ui::InputEvent wheel(float x, float y, float dx, float dy) {
    ui::InputEvent event{};
    event.type = ui::InputType::PointerWheel;
    event.position = {x, y};
    event.delta = {dx, dy};
    return event;
}

int self_test() {
    example::Platform platform;

    // Nested scrolling: the inner horizontal ScrollView consumes while it can
    // move. At its horizontal boundary the same wheel event bubbles to the
    // outer vertical ScrollView, which consumes the remaining enabled-axis move.
    ui::ScrollState outer{ui::ScrollAxis::Vertical};
    ui::ScrollState inner{ui::ScrollAxis::Horizontal};
    ui::UI nested{
        ui::ScrollView{
            outer,
            ui::Column{
                ui::ScrollView{inner, ui::Spacer{400.0f, 40.0f}},
                ui::Spacer{100.0f, 300.0f}
            }.gap(0.0f).padding(0.0f)}};
    nested.resize({100.0f, 100.0f});
    nested.activate(platform);

    if (!example::near(inner.max_offset().x, 300.0f) ||
        !example::near(outer.max_offset().y, 240.0f)) {
        return example::fail("nested ScrollView metrics are incorrect");
    }
    if (nested.dispatch(wheel(20.0f, 20.0f, 40.0f, 0.0f), platform) !=
            ui::EventResult::Handled ||
        !example::near(inner.offset().x, 40.0f) ||
        !example::near(outer.offset().y, 0.0f)) {
        return example::fail("inner ScrollView did not consume horizontal wheel input");
    }

    inner.set_offset({300.0f, 0.0f});
    if (nested.dispatch(wheel(20.0f, 20.0f, 40.0f, 30.0f), platform) !=
            ui::EventResult::Handled ||
        !example::near(inner.offset().x, 300.0f) ||
        !example::near(outer.offset().y, 30.0f)) {
        return example::fail("boundary wheel did not bubble to the outer ScrollView");
    }

    // Overlay thumb drag: with 100/400 viewport/content geometry the vertical
    // thumb is 25 px long and a captured drag maps linearly across max_offset.
    ui::ScrollState thumb_state{ui::ScrollAxis::Vertical};
    ui::UI thumb_tree{
        ui::ScrollView{thumb_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)};
    thumb_tree.resize({100.0f, 100.0f});
    thumb_tree.activate(platform);
    thumb_state.set_offset({0.0f, 100.0f});

    if (thumb_tree.dispatch(
            example::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) !=
        ui::EventResult::Handled) {
        return example::fail("scrollbar thumb did not capture PointerDown");
    }
    if (thumb_tree.dispatch(
            example::pointer(ui::InputType::PointerMove, 96.0f, 80.0f), platform) !=
            ui::EventResult::Handled ||
        !example::near(thumb_state.offset().y, 300.0f)) {
        return example::fail("scrollbar thumb drag did not map to max offset");
    }
    if (thumb_tree.dispatch(
            example::pointer(ui::InputType::PointerUp, 96.0f, 80.0f), platform) !=
        ui::EventResult::Handled) {
        return example::fail("scrollbar thumb did not release capture");
    }

    // Programmatic ensure_visible remains available and uses ScrollState as the
    // sole offset authority.
    thumb_state.set_offset({0.0f, 0.0f});
    if (!ui::ensure_visible(
            thumb_state, {0.0f, 250.0f, 50.0f, 20.0f}, ui::ScrollAlignment::Nearest) ||
        !example::near(thumb_state.offset().y, 170.0f)) {
        return example::fail("ensure_visible Nearest produced the wrong offset");
    }

    ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
    if (!renderer.render(thumb_tree)) {
        return example::fail("headless ScrollView render failed");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T034 ScrollView", {620.0f, 460.0f});
}
