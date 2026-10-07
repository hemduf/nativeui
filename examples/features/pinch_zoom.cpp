#include "example_support.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

// InputType::Magnify (PR #502): one continuous relative-scale zoom event on
// every backend. On macOS the trackpad pinch delivers it natively; on other
// platforms ctrl/cmd+scroll (the OS-synthesized pinch and the browser ctrlKey
// wheel) is normalized into it at the Pugl boundary, and PointerWheel never
// carries ctrl/gui after normalization. The example accumulates the delivered
// factors into a scale and keeps plain scrolling available as PointerWheel.
struct ZoomState {
    int magnify_events{};
    int wheel_events{};
    ui::Point last_position{};
    float last_factor{};
    float scale{1.0f};
};

class PinchZoomComponent final : public ui::Component {
public:
    explicit PinchZoomComponent(std::shared_ptr<ZoomState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {560.0f, 200.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::Magnify) {
            ++state_->magnify_events;
            state_->last_position = event.position;
            state_->last_factor = event.magnification;
            const float next = state_->scale * (1.0f + event.magnification);
            state_->scale = next < 0.25f ? 0.25f : next > 4.0f ? 4.0f : next;
            context.invalidate();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerWheel) {
            ++state_->wheel_events;
            context.invalidate();
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext& paint) const override {
        auto& painter = paint.painter();
        const auto bounds = paint.bounds();
        painter.fill_rounded_rect(bounds, 10.0f, ui::colors::panel);
        painter.stroke_rounded_rect(bounds, 10.0f, 1.0f, ui::colors::border);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 32.0f},
            "PINCH ZOOM INPUT EVENT",
            14.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 62.0f},
            "Pinch on the trackpad (macOS) or ctrl/cmd + scroll elsewhere:",
            11.0f,
            ui::colors::textMuted);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 82.0f},
            "both arrive as InputType::Magnify with a continuous factor.",
            11.0f,
            ui::colors::textMuted);

        const std::string events = state_->magnify_events == 0
            ? "Magnify events: 0"
            : "Magnify events: " + std::to_string(state_->magnify_events) +
                  "  at (" + std::to_string(static_cast<int>(state_->last_position.x)) + ", " +
                  std::to_string(static_cast<int>(state_->last_position.y)) + ")" +
                  "  last factor " + std::to_string(state_->last_factor);
        painter.text({bounds.x + 16.0f, bounds.y + 118.0f}, events, 11.0f, ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 142.0f},
            "Accumulated scale: " + std::to_string(state_->scale),
            11.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 166.0f},
            "Plain wheel events: " + std::to_string(state_->wheel_events) +
                " (ctrl/cmd scroll is reserved for zoom)",
            11.0f,
            ui::colors::textMuted);
    }

private:
    std::shared_ptr<ZoomState> state_;
};

class PinchZoomProbe {
public:
    explicit PinchZoomProbe(std::shared_ptr<ZoomState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<PinchZoomComponent>(state); },
            {}};
    }

private:
    std::shared_ptr<ZoomState> state_;
};

int run_self_test() {
    auto state = std::make_shared<ZoomState>();
    ui::UI tree{PinchZoomProbe{state}};
    example::Platform platform;
    tree.resize({600.0f, 240.0f});
    tree.activate(platform);

    ui::InputEvent zoom_in{};
    zoom_in.type = ui::InputType::Magnify;
    zoom_in.position = {240.0f, 96.0f};
    zoom_in.magnification = 0.5f;
    if (tree.dispatch(zoom_in, platform) != ui::EventResult::Handled) {
        return example::fail("magnify event was not delivered to the hit target");
    }
    if (state->magnify_events != 1) {
        return example::fail("hit target did not observe the magnify event");
    }
    if (!example::near(state->scale, 1.5f) || !example::near(state->last_factor, 0.5f)) {
        return example::fail("magnify factor was not applied to the accumulated scale");
    }
    if (!example::near(state->last_position.x, 240.0f) ||
        !example::near(state->last_position.y, 96.0f)) {
        return example::fail("magnify position was not preserved");
    }

    ui::InputEvent zoom_out{};
    zoom_out.type = ui::InputType::Magnify;
    zoom_out.position = {200.0f, 40.0f};
    zoom_out.magnification = -0.5f;
    if (tree.dispatch(zoom_out, platform) != ui::EventResult::Handled) {
        return example::fail("negative magnify event was not delivered");
    }
    if (!example::near(state->scale, 0.75f)) {
        return example::fail("negative magnify factor was not applied");
    }

    // Plain scroll stays available as PointerWheel and never zooms.
    ui::InputEvent plain_wheel{};
    plain_wheel.type = ui::InputType::PointerWheel;
    plain_wheel.position = {240.0f, 96.0f};
    plain_wheel.delta = {0.0f, 1.0f};
    if (tree.dispatch(plain_wheel, platform) != ui::EventResult::Handled) {
        return example::fail("plain scroll was not delivered as PointerWheel");
    }
    if (state->wheel_events != 1 || state->magnify_events != 2 ||
        !example::near(state->scale, 0.75f)) {
        return example::fail("plain scroll was misrouted or changed the zoom state");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

    auto state = std::make_shared<ZoomState>();
    ui::UI tree{PinchZoomProbe{state}};
    return example::run_window(tree, "NativeUI pinch zoom input", {620.0f, 260.0f});
}
