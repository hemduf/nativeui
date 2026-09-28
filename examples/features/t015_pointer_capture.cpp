#include "example_support.hpp"

#include <algorithm>

namespace {
struct DragModel {
    ui::Point point{80.0f, 70.0f};
    bool dragging{};
    int cancels{};
};
}

int main(int argc, char** argv) {
    DragModel model;

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T015 / POINTER CAPTURE + CANCEL"},
                ui::Canvas{500.0f, 220.0f, [&](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                    g.stroke_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f,
                                          g.focused() ? 2.0f : 1.0f,
                                          g.focused() ? ui::colors::borderFocus : ui::colors::border);
                    g.circle(model.point, 22.0f, model.dragging ? ui::colors::accent : ui::colors::toggleOff);
                    g.text({14.0f, 18.0f}, "Drag the circle. Focus loss/host close cancels the gesture safely.",
                           11.0f, ui::colors::textMuted);
                    g.text({14.0f, 198.0f}, "cancel events: " + std::to_string(model.cancels),
                           10.0f, ui::colors::textMuted);
                }}.on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    if (event.type == ui::InputType::PointerDown) {
                        model.dragging = true;
                        model.point = event.position;
                        ctx.capture_pointer();
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerMove && model.dragging) {
                        model.point = event.position;
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerUp && model.dragging) {
                        model.dragging = false;
                        // Explicit release is optional on PointerUp; the tree auto-releases.
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::PointerCancel && model.dragging) {
                        model.dragging = false;
                        ++model.cancels;
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
        tree->resize({540.0f, 300.0f});
        tree->activate(platform);
        tree->dispatch(example::pointer(ui::InputType::PointerDown, 80.0f, 140.0f), platform);
        if (!model.dragging) return example::fail("drag did not start");
        tree->dispatch(example::pointer(ui::InputType::PointerMove, 900.0f, 900.0f), platform);
        if (!(model.point.x > 500.0f)) return example::fail("captured move did not route outside bounds");
        tree->deactivate(platform);
        if (model.dragging || model.cancels != 1) return example::fail("deactivate did not cancel capture exactly once");
        tree->deactivate(platform);
        if (model.cancels != 1) return example::fail("capture cancellation repeated");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T015 - Pointer Capture", {560.0f, 360.0f});
}
