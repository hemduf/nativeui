#include "example_support.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

// T175: the platform forwards the right button as InputType::ContextMenu. The
// example records the delivered request; run it and right-click inside the
// panel to see the position and modifiers, then compare with a left click
// (which must not produce a context-menu request).
struct MenuState {
    int context_requests{};
    int pointer_presses{};
    ui::Point last_position{};
    bool shift{};
    bool primary{};
};

class ContextMenuProbeComponent final : public ui::Component {
public:
    explicit ContextMenuProbeComponent(std::shared_ptr<MenuState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {560.0f, 180.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) ++state_->pointer_presses;
        if (event.type != ui::InputType::ContextMenu) return ui::EventResult::Ignored;
        ++state_->context_requests;
        state_->last_position = event.position;
        state_->shift = event.shift;
        state_->primary = event.primary;
        context.invalidate();
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext& paint) const override {
        auto& painter = paint.painter();
        const auto bounds = paint.bounds();
        painter.fill_rounded_rect(bounds, 10.0f, ui::colors::panel);
        painter.stroke_rounded_rect(bounds, 10.0f, 1.0f, ui::colors::border);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 32.0f},
            "T175 / CONTEXT-MENU INPUT EVENT",
            14.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 62.0f},
            "Right-click inside this panel: it becomes InputType::ContextMenu.",
            11.0f,
            ui::colors::textMuted);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 90.0f},
            "Focus stays put; active capture is cancelled before menu routing.",
            11.0f,
            ui::colors::textMuted);

        const std::string status = state_->context_requests == 0
            ? "Context-menu requests: 0"
            : "Context-menu requests: " + std::to_string(state_->context_requests) +
                  "  at (" + std::to_string(static_cast<int>(state_->last_position.x)) + ", " +
                  std::to_string(static_cast<int>(state_->last_position.y)) + ")" +
                  "  shift=" + (state_->shift ? "true" : "false") +
                  "  primary=" + (state_->primary ? "true" : "false");
        painter.text({bounds.x + 16.0f, bounds.y + 128.0f}, status, 11.0f, ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 152.0f},
            "Pointer presses: " + std::to_string(state_->pointer_presses),
            11.0f,
            ui::colors::textMuted);
    }

private:
    std::shared_ptr<MenuState> state_;
};

class ContextMenuProbe {
public:
    explicit ContextMenuProbe(std::shared_ptr<MenuState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ContextMenuProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<MenuState> state_;
};

int run_self_test() {
    auto state = std::make_shared<MenuState>();
    ui::UI tree{ContextMenuProbe{state}};
    example::Platform platform;
    tree.resize({600.0f, 220.0f});
    tree.activate(platform);

    auto request = ui::InputEvent{};
    request.type = ui::InputType::ContextMenu;
    request.position = {240.0f, 96.0f};
    request.shift = true;
    request.primary = true;
    if (tree.dispatch(request, platform) != ui::EventResult::Handled) {
        return example::fail("context-menu request was not delivered to the hit target");
    }
    if (state->context_requests != 1) {
        return example::fail("hit target did not observe the context-menu request");
    }
    if (!example::near(state->last_position.x, 240.0f) ||
        !example::near(state->last_position.y, 96.0f)) {
        return example::fail("context-menu position was not preserved");
    }
    if (!state->shift || !state->primary) {
        return example::fail("context-menu modifiers were not preserved");
    }

    auto press = ui::InputEvent{};
    press.type = ui::InputType::PointerDown;
    press.position = {120.0f, 64.0f};
    (void)tree.dispatch(press, platform);
    if (state->pointer_presses != 1 || state->context_requests != 1) {
        return example::fail("pointer press changed the context-menu accounting");
    }

    auto outside = ui::InputEvent{};
    outside.type = ui::InputType::ContextMenu;
    outside.position = {2000.0f, 2000.0f};
    if (tree.dispatch(outside, platform) != ui::EventResult::Ignored) {
        return example::fail("out-of-bounds context-menu request was not ignored");
    }
    if (state->context_requests != 1) {
        return example::fail("out-of-bounds request reached the target");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

    auto state = std::make_shared<MenuState>();
    ui::UI tree{ContextMenuProbe{state}};
    return example::run_window(tree, "NativeUI T175 - Context menu input", {620.0f, 240.0f});
}
