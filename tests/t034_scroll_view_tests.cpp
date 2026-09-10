#include "test_support.hpp"

namespace {

ui::InputEvent wheel(float x, float y, float dx, float dy) {
    ui::InputEvent event{};
    event.type = ui::InputType::PointerWheel;
    event.position = {x, y};
    event.delta = {dx, dy};
    return event;
}

bool accent_pixel(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g > 120 && pixel.g < 190 && pixel.b < 100 && pixel.a > 220;
}

bool darker_track_than_background(ui::Rgba8 track, ui::Rgba8 background) {
    return track.r > background.r && track.g > background.g && track.b > background.b &&
           track.a == background.a;
}

struct PointerEatingState {
    int down{};
    int move{};
    int up{};
};

class PointerEatingComponent final : public ui::Component {
public:
    explicit PointerEatingComponent(std::shared_ptr<PointerEatingState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 400.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++state_->down;
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            ++state_->up;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<PointerEatingState> state_;
};

class PointerEatingContent {
public:
    explicit PointerEatingContent(std::shared_ptr<PointerEatingState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<PointerEatingComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<PointerEatingState> state_;
};

void wheel_consumption_uses_scroll_state() {
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK_NEAR(state.max_offset().y, 300.0f, 0.001f);
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 40.0f, 0.001f);

    state.set_offset({0.0f, 300.0f});
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);

    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 0.0f, -25.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 275.0f, 0.001f);
}

void nested_wheel_bubbles_at_boundary() {
    ui::ScrollState outer{ui::ScrollAxis::Vertical};
    ui::ScrollState inner{ui::ScrollAxis::Horizontal};
    ui::UI tree{
        ui::ScrollView{
            outer,
            ui::Column{
                ui::ScrollView{inner, ui::Spacer{400.0f, 40.0f}},
                ui::Spacer{100.0f, 300.0f}
            }.gap(0.0f).padding(0.0f)}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK_NEAR(inner.max_offset().x, 300.0f, 0.001f);
    NUI_CHECK_NEAR(outer.max_offset().y, 240.0f, 0.001f);
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 40.0f, 0.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(inner.offset().x, 40.0f, 0.001f);
    NUI_CHECK_NEAR(outer.offset().y, 0.0f, 0.001f);

    inner.set_offset({300.0f, 0.0f});
    NUI_CHECK(tree.dispatch(wheel(20.0f, 20.0f, 40.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(inner.offset().x, 300.0f, 0.001f);
    NUI_CHECK_NEAR(outer.offset().y, 30.0f, 0.001f);
}

void pointer_pan_is_opt_in_and_cancellable() {
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Ignored);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 50.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 50.0f, 20.0f), platform);
    NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);

    ui::ScrollState pannable_state{ui::ScrollAxis::Vertical};
    ui::UI pannable{
        ui::ScrollView{pannable_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)};
    pannable.resize({100.0f, 100.0f});
    pannable.activate(platform);

    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, -30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(pannable_state.offset().y, 100.0f, 0.001f);
    NUI_CHECK(pannable.cancel_pointer(platform) == ui::EventResult::Handled);
    const auto cancelled_offset = pannable_state.offset().y;
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, -80.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(pannable_state.offset().y, cancelled_offset, 0.001f);

    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(pannable.dispatch(
                  test::pointer(ui::InputType::PointerUp, 50.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
}

void scrollbar_geometry_and_capture_contract() {
    const auto hidden = ui::detail::scroll_view_bars_for_metrics(
        ui::ScrollAxis::Both, {}, {100.0f, 100.0f}, {100.0f, 100.0f},
        {0.0f, 0.0f, 100.0f, 100.0f});
    NUI_CHECK(!hidden.horizontal.visible);
    NUI_CHECK(!hidden.vertical.visible);

    const auto horizontal = ui::detail::scroll_view_bars_for_metrics(
        ui::ScrollAxis::Horizontal, {}, {100.0f, 100.0f}, {400.0f, 100.0f},
        {0.0f, 0.0f, 100.0f, 100.0f});
    NUI_CHECK(horizontal.horizontal.visible);
    NUI_CHECK(!horizontal.vertical.visible);
    NUI_CHECK_NEAR(horizontal.horizontal.track.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(horizontal.horizontal.thumb.w, 25.0f, 0.001f);

    const auto both = ui::detail::scroll_view_bars_for_metrics(
        ui::ScrollAxis::Both, {}, {100.0f, 100.0f}, {10000.0f, 10000.0f},
        {0.0f, 0.0f, 100.0f, 100.0f});
    NUI_CHECK(both.horizontal.visible);
    NUI_CHECK(both.vertical.visible);
    NUI_CHECK_NEAR(both.horizontal.track.w, 92.0f, 0.001f);
    NUI_CHECK_NEAR(both.vertical.track.h, 92.0f, 0.001f);
    NUI_CHECK_NEAR(both.horizontal.thumb.w, 18.0f, 0.001f);
    NUI_CHECK_NEAR(both.vertical.thumb.h, 18.0f, 0.001f);

    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);
    state.set_offset({0.0f, 100.0f});

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 180.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);
    NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 20.0f), platform) ==
              ui::EventResult::Ignored);

    state.set_offset({0.0f, 100.0f});
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 90.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 40.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
}

void t059_availability_contract() {
    test::MockPlatform platform;

    ui::State<bool> read_only{true};
    ui::ScrollState readable_state{ui::ScrollAxis::Vertical};
    ui::UI readable{
        ui::ReadOnly{read_only,
            ui::ScrollView{readable_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)}};
    readable.resize({100.0f, 100.0f});
    readable.activate(platform);
    NUI_CHECK(readable.dispatch(wheel(20.0f, 20.0f, 0.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(readable_state.offset().y, 30.0f, 0.001f);
    NUI_CHECK(readable.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(readable.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(readable_state.offset().y, 80.0f, 0.001f);
    NUI_CHECK(readable.cancel_pointer(platform) == ui::EventResult::Handled);

    ui::State<bool> enabled{false};
    ui::ScrollState disabled_state{ui::ScrollAxis::Vertical};
    ui::UI disabled{
        ui::Enabled{enabled,
            ui::ScrollView{disabled_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)}};
    disabled.resize({100.0f, 100.0f});
    disabled.activate(platform);
    NUI_CHECK(disabled.dispatch(wheel(20.0f, 20.0f, 0.0f, 30.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(disabled.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(disabled.dispatch(
                  test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(disabled.dispatch(
                  test::pointer(ui::InputType::PointerDown, 96.0f, 90.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(disabled_state.offset().y, 0.0f, 0.001f);
}

void t059_mid_interaction_unavailability_cancels_capture() {
    test::MockPlatform platform;

    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    ui::ScrollState pan_state{ui::ScrollAxis::Vertical};
    ui::UI pan_tree{
        ui::Visibility{visibility,
            ui::ScrollView{pan_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)}};
    pan_tree.resize({100.0f, 100.0f});
    pan_tree.activate(platform);

    NUI_CHECK(pan_tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    visibility.set(ui::VisibilityMode::Hidden);
    NUI_CHECK(pan_tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    visibility.set(ui::VisibilityMode::Visible);
    const auto hidden_cancel_offset = pan_state.offset().y;
    NUI_CHECK(pan_tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 50.0f, 10.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(pan_state.offset().y, hidden_cancel_offset, 0.001f);

    NUI_CHECK(pan_tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 50.0f, 70.0f), platform) ==
              ui::EventResult::Handled);
    visibility.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK(pan_tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    visibility.set(ui::VisibilityMode::Visible);

    ui::State<bool> enabled{true};
    ui::ScrollState thumb_state{ui::ScrollAxis::Vertical};
    ui::UI thumb_tree{
        ui::Enabled{enabled,
            ui::ScrollView{thumb_state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)}};
    thumb_tree.resize({100.0f, 100.0f});
    thumb_tree.activate(platform);
    thumb_state.set_offset({0.0f, 100.0f});

    NUI_CHECK(thumb_tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    enabled.set(false);
    NUI_CHECK(thumb_tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    enabled.set(true);
    const auto disabled_cancel_offset = thumb_state.offset().y;
    NUI_CHECK(thumb_tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 96.0f, 80.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK_NEAR(thumb_state.offset().y, disabled_cancel_offset, 0.001f);
}

void scrollbar_overlay_wins_over_interactive_content() {
    auto content = std::make_shared<PointerEatingState>();
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{
        ui::ScrollView{state, PointerEatingContent{content}}.pointer_pan(true)};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);
    state.set_offset({0.0f, 100.0f});

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(content->down == 0);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 80.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerUp, 96.0f, 80.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(content->move == 0);
    NUI_CHECK(content->up == 0);
}

void inert_overlay_does_not_block_pointer_target() {
    int activations = 0;
    ui::UI tree{
        ui::Stack{
            ui::Button{"under", [&] { ++activations; }},
            ui::Spacer{100.0f, 100.0f}}};
    test::MockPlatform platform;
    tree.resize({100.0f, 100.0f});
    tree.activate(platform);

    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(activations == 1);
}

void idle_scroll_view_schedules_no_activity() {
    ui::ScrollState state{ui::ScrollAxis::Vertical};
    ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
    ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    test::MockPlatform platform;
    tree.activate(platform);
    NUI_CHECK(tree.dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    ui::InputEvent tick{};
    tick.type = ui::InputType::Tick;
    NUI_CHECK(tree.dispatch(tick, platform) == ui::EventResult::Ignored);
    NUI_CHECK(!tree.dirty());
}

void headless_scrollbar_golden_states() {
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        state.set_offset({0.0f, 100.0f});
        NUI_CHECK(renderer.render(tree));

        const auto background = renderer.pixel(50, 50);
        NUI_CHECK(accent_pixel(renderer.pixel(96, 30)));
        NUI_CHECK(darker_track_than_background(renderer.pixel(96, 55), background));
        NUI_CHECK(!accent_pixel(renderer.pixel(50, 96)));
    }

    {
        ui::ScrollState state{ui::ScrollAxis::Horizontal};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{400.0f, 100.0f}}};
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        state.set_offset({100.0f, 0.0f});
        NUI_CHECK(renderer.render(tree));

        const auto background = renderer.pixel(50, 50);
        NUI_CHECK(accent_pixel(renderer.pixel(30, 96)));
        NUI_CHECK(darker_track_than_background(renderer.pixel(55, 96), background));
        NUI_CHECK(!accent_pixel(renderer.pixel(96, 50)));
    }

    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{400.0f, 400.0f}}};
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        state.set_offset({100.0f, 100.0f});
        NUI_CHECK(renderer.render(tree));

        const auto background = renderer.pixel(50, 50);
        NUI_CHECK(accent_pixel(renderer.pixel(30, 96)));
        NUI_CHECK(accent_pixel(renderer.pixel(96, 30)));
        NUI_CHECK(darker_track_than_background(renderer.pixel(60, 96), background));
        NUI_CHECK(darker_track_than_background(renderer.pixel(96, 60), background));
        const auto corner = renderer.pixel(96, 96);
        NUI_CHECK(corner.r == background.r && corner.g == background.g &&
                  corner.b == background.b && corner.a == background.a);
    }
}

void suite() {
    wheel_consumption_uses_scroll_state();
    nested_wheel_bubbles_at_boundary();
    pointer_pan_is_opt_in_and_cancellable();
    scrollbar_geometry_and_capture_contract();
    t059_availability_contract();
    t059_mid_interaction_unavailability_cancels_capture();
    scrollbar_overlay_wins_over_interactive_content();
    inert_overlay_does_not_block_pointer_target();
    idle_scroll_view_schedules_no_activity();
    headless_scrollbar_golden_states();
}

} // namespace

int main() { return test::run("t034_scroll_view", &suite); }
