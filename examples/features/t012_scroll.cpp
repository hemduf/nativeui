#include "example_support.hpp"

int main(int argc, char** argv) {
    ui::ScrollState scroll{ui::ScrollAxis::Vertical};

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T012 / SCROLL LAYOUT"},
                ui::Canvas{520.0f, 45.0f, [&](ui::CanvasContext2D& g) {
                    g.text({0.0f, 16.0f}, "Focus here, then use Up/Down to change ScrollState offset.", 11.0f, ui::colors::textMuted);
                }}.on_input([&](const ui::InputEvent& e, ui::CanvasInputContext& ctx) {
                    if (e.type != ui::InputType::KeyDown) return ui::EventResult::Ignored;
                    if (e.key == ui::Key::Down) scroll.scroll_by({0.0f, 30.0f});
                    else if (e.key == ui::Key::Up) scroll.scroll_by({0.0f, -30.0f});
                    else return ui::EventResult::Ignored;
                    ctx.invalidate_layout();
                    return ui::EventResult::Handled;
                }),
                ui::Flex{
                    ui::Scroll{scroll,
                        ui::Column{
                            example::Box{"item 1", {500.0f, 60.0f}, ui::colors::accent},
                            example::Box{"item 2", {500.0f, 60.0f}, ui::colors::toggleOff},
                            example::Box{"item 3", {500.0f, 60.0f}, ui::colors::accent},
                            example::Box{"item 4", {500.0f, 60.0f}, ui::colors::toggleOff},
                            example::Box{"item 5", {500.0f, 60.0f}, ui::colors::accent}}
                            .padding(0.0f).gap(8.0f)}}
                    .grow(1.0f).shrink(1.0f)
            }.padding(18.0f).gap(10.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        ui::HeadlessRenderer renderer{{560.0f, 240.0f}};
        if (!renderer.render(*tree)) return example::fail("headless render failed");
        if (!(scroll.max_offset().y > 0.0f)) return example::fail("scroll content did not overflow viewport");
        scroll.set_offset({0.0f, 10000.0f});
        if (!example::near(scroll.offset().y, scroll.max_offset().y)) return example::fail("scroll offset did not clamp");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T012 - Scroll Layout", {600.0f, 430.0f});
}
