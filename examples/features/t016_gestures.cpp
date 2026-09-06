#include "example_support.hpp"

namespace {
struct Model {
    ui::Point knob{250.0f, 105.0f};
    ui::DragGesture drag{5.0f};
    float value{0.5f};
    float start_value{0.5f};
    int clicks{};
};
}

int main(int argc, char** argv) {
    Model model;

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T016 / REUSABLE CLICK + DRAG GESTURES"},
                ui::Canvas{520.0f, 210.0f, [&](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                    const float x = 35.0f + model.value * 450.0f;
                    g.line({35.0f, 105.0f}, {485.0f, 105.0f}, 4.0f, ui::colors::track);
                    g.circle({x, 105.0f}, 18.0f, model.drag.dragging() ? ui::colors::accent : ui::colors::toggleOff);
                    g.text({18.0f, 24.0f}, "Click or drag horizontally. Drag starts after a 5 px threshold.", 11.0f, ui::colors::textMuted);
                    g.text({18.0f, 188.0f}, "value: " + std::to_string(model.value) + "  clicks: " + std::to_string(model.clicks), 10.0f, ui::colors::textMuted);
                }}.on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    if (event.type == ui::InputType::PointerDown) {
                        model.drag.begin(event.position);
                        model.start_value = model.value;
                        ctx.capture_pointer();
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerMove && model.drag.active()) {
                        const auto drag = model.drag.move(event.position);
                        if (drag.dragging) {
                            model.value = std::clamp(
                                model.start_value + ui::drag_value_delta(drag.total, ui::DragAxis::Horizontal, 1.0f / 450.0f),
                                0.0f,
                                1.0f);
                        }
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerUp && model.drag.active()) {
                        const auto end = model.drag.end(event.position);
                        if (end.clicked) ++model.clicks;
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerCancel && model.drag.active()) {
                        (void)model.drag.cancel();
                        model.value = model.start_value;
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    return ui::EventResult::Ignored;
                })
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({560.0f, 320.0f});
        tree->activate(platform);

        tree->dispatch(example::pointer(ui::InputType::PointerDown, 100.0f, 160.0f), platform);
        tree->dispatch(example::pointer(ui::InputType::PointerMove, 103.0f, 162.0f), platform);
        if (model.drag.dragging()) return example::fail("drag crossed threshold too early");
        tree->dispatch(example::pointer(ui::InputType::PointerUp, 103.0f, 162.0f), platform);
        if (model.clicks != 1) return example::fail("click was not reported");

        tree->dispatch(example::pointer(ui::InputType::PointerDown, 100.0f, 160.0f), platform);
        tree->dispatch(example::pointer(ui::InputType::PointerMove, 145.0f, 160.0f), platform);
        if (!model.drag.dragging() || !(model.value > 0.55f)) return example::fail("drag/value mapping failed");
        tree->dispatch(example::pointer(ui::InputType::PointerUp, 145.0f, 160.0f), platform);
        if (model.clicks != 1) return example::fail("drag was misclassified as click");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T016 - Gestures", {580.0f, 360.0f});
}
