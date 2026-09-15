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

        // T127: the public Scroll builder and its realized retained component
        // borrow ScrollState without owning it. A dead owner must make both the
        // not-yet-realized factory and an already-realized tree inert rather
        // than leaving a dangling retained dereference.
        {
            auto state = std::make_unique<ui::ScrollState>(ui::ScrollAxis::Vertical);
            auto spec = std::move(ui::Scroll{*state, ui::Spacer{100.0f, 400.0f}}).spec();
            state.reset();
            auto component = spec.factory();
            const std::vector<ui::ChildMetrics> child_metrics{
                ui::ChildMetrics{{100.0f, 400.0f}}};
            const auto minimum = component->minimum_size(child_metrics);
            if (!example::near(minimum.h, 0.0f)) {
                return example::fail("dead ScrollState factory did not stay lifetime-safe");
            }
        }

        {
            auto state = std::make_unique<ui::ScrollState>(ui::ScrollAxis::Vertical);
            ui::UI retained{ui::Scroll{*state, ui::Spacer{100.0f, 400.0f}}};
            ui::HeadlessRenderer retained_renderer{{100.0f, 100.0f}};
            if (!retained_renderer.render(retained)) {
                return example::fail("retained Scroll lifetime setup render failed");
            }
            state.reset();
            retained_renderer.resize({120.0f, 120.0f});
            if (!retained_renderer.render(retained)) {
                return example::fail("retained Scroll dereferenced a dead ScrollState");
            }
        }

        // T127: the still-public ScrollComponent constructor also accepts a
        // borrowed ScrollState directly. It must freeze immutable axis data and
        // re-check owner lifetime after update_metrics() synchronously invokes
        // observers, because one of those observers may destroy the state.
        {
            auto state = std::make_unique<ui::ScrollState>(ui::ScrollAxis::Vertical);
            ui::ScrollComponent component{*state};
            const std::vector<ui::ChildMetrics> child_metrics{
                ui::ChildMetrics{{100.0f, 400.0f}}};
            std::vector<ui::ChildPlacement> placements(1);

            state->set_offset({0.0f, 250.0f});
            auto destroyer = state->observe([&](ui::Point) { state.reset(); });
            component.layout_children(
                {0.0f, 0.0f, 100.0f, 300.0f}, child_metrics, placements);

            if (state) {
                return example::fail("ScrollComponent clamp observer did not destroy owner");
            }
            if (!example::near(placements.front().bounds.x, 0.0f) ||
                !example::near(placements.front().bounds.y, 0.0f) ||
                !example::near(placements.front().bounds.w, 100.0f) ||
                !example::near(placements.front().bounds.h, 400.0f)) {
                return example::fail("dead ScrollComponent did not fall back to safe placement");
            }

            const auto minimum = component.minimum_size(child_metrics);
            if (!example::near(minimum.h, 0.0f)) {
                return example::fail("dead ScrollComponent dereferenced ScrollState in sizing");
            }
            component.layout_children(
                {0.0f, 0.0f, 120.0f, 320.0f}, child_metrics, placements);
            if (!example::near(placements.front().bounds.x, 0.0f) ||
                !example::near(placements.front().bounds.y, 0.0f)) {
                return example::fail("dead ScrollComponent was not inert on later layout");
            }
            destroyer.reset();
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T012 - Scroll Layout", {600.0f, 430.0f});
}