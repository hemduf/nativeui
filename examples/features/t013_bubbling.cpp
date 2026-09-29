#include "example_support.hpp"

#include <memory>

namespace {

struct BubbleState {
    int parent_events{};
    int leaf_events{};
};

class BubbleParentComponent final : public ui::Component {
public:
    explicit BubbleParentComponent(std::shared_ptr<BubbleState> state) : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{360.0f, 140.0f} : children.front().preferred;
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& ctx) override {
        if (event.type == ui::InputType::PointerDown || event.type == ui::InputType::KeyDown) {
            ++state_->parent_events;
            ctx.invalidate();
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext& context) const override {
        const auto b = context.bounds();
        auto& p = context.painter();
        p.stroke_rounded_rect(b, 12.0f, 2.0f, ui::colors::accent);
        p.text({b.x + 12.0f, b.y + 18.0f},
               "parent handled: " + std::to_string(state_->parent_events),
               11.0f,
               ui::colors::textMuted);
    }

private:
    std::shared_ptr<BubbleState> state_;
};

class BubbleParent {
public:
    template <class Child>
    BubbleParent(std::shared_ptr<BubbleState> state, Child&& child) : state_(std::move(state)) {
        children_.push_back(ui::make_spec(std::forward<Child>(child)));
    }

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state] { return std::make_unique<BubbleParentComponent>(state); },
            std::move(children_)};
    }

private:
    std::shared_ptr<BubbleState> state_;
    std::vector<ui::Spec> children_;
};

} // namespace

int main(int argc, char** argv) {
    auto state = std::make_shared<BubbleState>();

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T013 / INPUT BUBBLING"},
                BubbleParent{state,
                    ui::Canvas{420.0f, 150.0f, [state](ui::CanvasContext2D& g) {
                        g.fill_rounded_rect({24.0f, 36.0f, 372.0f, 90.0f}, 10.0f, ui::colors::panel);
                        g.text({210.0f, 70.0f}, "LEAF returns Ignored", 13.0f, ui::colors::text, ui::TextAlign::Center);
                        g.text({210.0f, 98.0f}, "click or press a key", 11.0f, ui::colors::textMuted, ui::TextAlign::Center);
                        g.text({210.0f, 122.0f}, "leaf events: " + std::to_string(state->leaf_events),
                               10.0f, ui::colors::textMuted, ui::TextAlign::Center);
                    }}.on_input([state](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                        if (event.type == ui::InputType::PointerDown || event.type == ui::InputType::KeyDown) {
                            ++state->leaf_events;
                            ctx.invalidate();
                            return ui::EventResult::Ignored;
                        }
                        return ui::EventResult::Ignored;
                    })}
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({480.0f, 240.0f});
        tree->activate(platform);
        const auto result = tree->dispatch(example::pointer(ui::InputType::PointerDown, 80.0f, 120.0f), platform);
        if (result != ui::EventResult::Handled) return example::fail("bubbled event was not handled by parent");
        if (state->leaf_events != 1 || state->parent_events != 1) return example::fail("target/parent routing counts incorrect");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T013 - Input Bubbling", {520.0f, 300.0f});
}
