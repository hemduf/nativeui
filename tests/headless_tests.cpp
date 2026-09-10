#include "test_support.hpp"

namespace {

struct OverlayProbeState {
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
};

class OverlayProbeComponent final : public ui::Component {
public:
    explicit OverlayProbeComponent(std::shared_ptr<OverlayProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {24.0f, 16.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void activate(ui::LifecycleContext&) override { ++state_->activates; }
    void deactivate(ui::LifecycleContext&) override { ++state_->deactivates; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<OverlayProbeState> state_;
};

class OverlayProbe {
public:
    explicit OverlayProbe(std::shared_ptr<OverlayProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<OverlayProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<OverlayProbeState> state_;
};

struct PointerProbeState {
    int pointer_downs{};
};

class PointerProbeComponent final : public ui::Component {
public:
    PointerProbeComponent(ui::Size size, std::shared_ptr<PointerProbeState> state)
        : size_(size), state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return size_;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        ++state_->pointer_downs;
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    ui::Size size_{};
    std::shared_ptr<PointerProbeState> state_;
};

class PointerProbe {
public:
    PointerProbe(ui::Size size, std::shared_ptr<PointerProbeState> state)
        : size_(size), state_(std::move(state)) {}

    ui::Spec spec() && {
        const auto size = size_;
        auto state = std::move(state_);
        return ui::Spec{
            [size, state = std::move(state)] {
                return std::make_unique<PointerProbeComponent>(size, state);
            },
            {}};
    }

private:
    ui::Size size_{};
    std::shared_ptr<PointerProbeState> state_;
};

struct EscapeProbeState {
    int escape_downs{};
};

class EscapeProbeComponent final : public ui::Component {
public:
    explicit EscapeProbeComponent(std::shared_ptr<EscapeProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::KeyDown || event.key != ui::Key::Escape) {
            return ui::EventResult::Ignored;
        }
        ++state_->escape_downs;
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<EscapeProbeState> state_;
};

class EscapeProbe {
public:
    explicit EscapeProbe(std::shared_ptr<EscapeProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<EscapeProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<EscapeProbeState> state_;
};

ui::OverlaySpec centered_overlay(ui::Spec content) {
    ui::OverlaySpec spec;
    spec.placement = ui::OverlayPlacement::Center;
    spec.content = std::move(content);
    return spec;
}

ui::OverlaySpec centered_pointer_overlay(
    const std::shared_ptr<PointerProbeState>& state,
    ui::OverlayPointerPolicy policy = ui::OverlayPointerPolicy::Normal) {
    auto spec = centered_overlay(ui::make_spec(PointerProbe{{24.0f, 16.0f}, state}));
    spec.pointer_policy = policy;
    return spec;
}

void check_overlay_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK(actual.x == expected.x);
    NUI_CHECK(actual.y == expected.y);
    NUI_CHECK(actual.w == expected.w);
    NUI_CHECK(actual.h == expected.h);
}

void overlay_placement_contract() {
    const ui::Rect viewport{0.0f, 0.0f, 100.0f, 100.0f};
    const ui::Rect anchor{40.0f, 40.0f, 10.0f, 10.0f};
    const ui::Size content{20.0f, 10.0f};

    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::AnchorBelow),
        {40.0f, 50.0f, 20.0f, 10.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::AnchorAbove),
        {40.0f, 30.0f, 20.0f, 10.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::AnchorRight),
        {50.0f, 40.0f, 20.0f, 10.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::AnchorLeft),
        {20.0f, 40.0f, 20.0f, 10.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::Center),
        {40.0f, 45.0f, 20.0f, 10.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, content, ui::OverlayPlacement::Auto),
        {40.0f, 50.0f, 20.0f, 10.0f});

    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport,
            {40.0f, 95.0f, 10.0f, 5.0f},
            {20.0f, 20.0f},
            ui::OverlayPlacement::AnchorBelow),
        {40.0f, 75.0f, 20.0f, 20.0f});

    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport,
            {40.0f, 95.0f, 10.0f, 5.0f},
            {20.0f, 20.0f},
            ui::OverlayPlacement::Auto),
        {40.0f, 75.0f, 20.0f, 20.0f});

    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport,
            {20.0f, 45.0f, 10.0f, 10.0f},
            {60.0f, 60.0f},
            ui::OverlayPlacement::AnchorBelow),
        {20.0f, 40.0f, 60.0f, 60.0f});
    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport,
            {20.0f, 45.0f, 10.0f, 10.0f},
            {60.0f, 60.0f},
            ui::OverlayPlacement::AnchorAbove),
        {20.0f, 0.0f, 60.0f, 60.0f});

    check_overlay_rect(
        ui::detail::overlay_placement_bounds(
            viewport, anchor, {140.0f, 120.0f}, ui::OverlayPlacement::Center),
        {0.0f, 0.0f, 140.0f, 120.0f});
}

void overlay_pointer_stack_contract() {
    test::MockPlatform platform;
    auto root = std::make_shared<PointerProbeState>();
    auto lower = std::make_shared<PointerProbeState>();
    auto upper = std::make_shared<PointerProbeState>();
    auto ignored = std::make_shared<PointerProbeState>();

    ui::UI tree{PointerProbe{{96.0f, 48.0f}, root}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    const auto lower_handle = tree.show_overlay(centered_pointer_overlay(lower));
    const auto upper_handle = tree.show_overlay(centered_pointer_overlay(upper));
    tree.resize({96.0f, 48.0f});

    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {48.0f, 24.0f};
    NUI_CHECK(ui::handled(tree.dispatch(down, platform)));
    NUI_CHECK(upper->pointer_downs == 1);
    NUI_CHECK(lower->pointer_downs == 0);
    NUI_CHECK(root->pointer_downs == 0);

    NUI_CHECK(tree.close_overlay(upper_handle));
    NUI_CHECK(tree.close_overlay(lower_handle));
    tree.resize({96.0f, 48.0f});

    const auto visible_handle = tree.show_overlay(centered_pointer_overlay(lower));
    const auto ignored_handle = tree.show_overlay(
        centered_pointer_overlay(ignored, ui::OverlayPointerPolicy::Ignore));
    tree.resize({96.0f, 48.0f});

    NUI_CHECK(ui::handled(tree.dispatch(down, platform)));
    NUI_CHECK(ignored->pointer_downs == 0);
    NUI_CHECK(lower->pointer_downs == 1);
    NUI_CHECK(root->pointer_downs == 0);

    NUI_CHECK(tree.close_overlay(visible_handle));
    tree.resize({96.0f, 48.0f});

    NUI_CHECK(ui::handled(tree.dispatch(down, platform)));
    NUI_CHECK(ignored->pointer_downs == 0);
    NUI_CHECK(root->pointer_downs == 1);
    NUI_CHECK(tree.close_overlay(ignored_handle));
}

void overlay_modal_pointer_barrier_contract() {
    test::MockPlatform platform;
    auto root = std::make_shared<PointerProbeState>();
    auto modal = std::make_shared<PointerProbeState>();
    auto above = std::make_shared<PointerProbeState>();

    ui::UI tree{PointerProbe{{96.0f, 48.0f}, root}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    auto modal_spec = centered_pointer_overlay(modal);
    modal_spec.mode = ui::OverlayMode::Modal;
    const auto modal_handle = tree.show_overlay(std::move(modal_spec));
    tree.resize({96.0f, 48.0f});

    ui::InputEvent outside;
    outside.type = ui::InputType::PointerDown;
    outside.position = {4.0f, 4.0f};
    NUI_CHECK(ui::handled(tree.dispatch(outside, platform)));
    NUI_CHECK(root->pointer_downs == 0);
    NUI_CHECK(modal->pointer_downs == 0);

    const auto above_handle = tree.show_overlay(centered_pointer_overlay(above));
    tree.resize({96.0f, 48.0f});

    ui::InputEvent inside;
    inside.type = ui::InputType::PointerDown;
    inside.position = {48.0f, 24.0f};
    NUI_CHECK(ui::handled(tree.dispatch(inside, platform)));
    NUI_CHECK(above->pointer_downs == 1);
    NUI_CHECK(modal->pointer_downs == 0);
    NUI_CHECK(root->pointer_downs == 0);

    NUI_CHECK(tree.close_overlay(above_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(ui::handled(tree.dispatch(inside, platform)));
    NUI_CHECK(modal->pointer_downs == 1);
    NUI_CHECK(root->pointer_downs == 0);

    NUI_CHECK(tree.close_overlay(modal_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(ui::handled(tree.dispatch(outside, platform)));
    NUI_CHECK(root->pointer_downs == 1);
}

void overlay_dismissal_contract() {
    test::MockPlatform platform;
    auto root = std::make_shared<PointerProbeState>();
    auto dismissed = std::make_shared<PointerProbeState>();
    auto ignored = std::make_shared<PointerProbeState>();

    ui::UI tree{PointerProbe{{96.0f, 48.0f}, root}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    auto dismissable = centered_pointer_overlay(dismissed);
    dismissable.dismiss_on_outside_pointer_down = true;
    const auto dismissable_handle = tree.show_overlay(std::move(dismissable));

    auto transparent = centered_pointer_overlay(ignored, ui::OverlayPointerPolicy::Ignore);
    transparent.dismiss_on_outside_pointer_down = true;
    const auto transparent_handle = tree.show_overlay(std::move(transparent));
    tree.resize({96.0f, 48.0f});

    ui::InputEvent outside;
    outside.type = ui::InputType::PointerDown;
    outside.position = {4.0f, 4.0f};
    NUI_CHECK(ui::handled(tree.dispatch(outside, platform)));
    NUI_CHECK(!dismissable_handle.valid());
    NUI_CHECK(transparent_handle.valid());
    NUI_CHECK(root->pointer_downs == 0);
    NUI_CHECK(dismissed->pointer_downs == 0);
    NUI_CHECK(ignored->pointer_downs == 0);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(ui::handled(tree.dispatch(outside, platform)));
    NUI_CHECK(root->pointer_downs == 1);
    NUI_CHECK(tree.close_overlay(transparent_handle));

    auto root_keys = std::make_shared<EscapeProbeState>();
    ui::UI keys{EscapeProbe{root_keys}};
    keys.resize({96.0f, 48.0f});
    keys.activate(platform);

    auto lower_escape = centered_overlay(
        ui::make_spec(OverlayProbe{std::make_shared<OverlayProbeState>()}));
    lower_escape.dismiss_on_escape = true;
    const auto lower_escape_handle = keys.show_overlay(std::move(lower_escape));

    auto visual_only = centered_overlay(
        ui::make_spec(OverlayProbe{std::make_shared<OverlayProbeState>()}));
    visual_only.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    const auto visual_only_handle = keys.show_overlay(std::move(visual_only));
    keys.resize({96.0f, 48.0f});

    ui::InputEvent escape;
    escape.type = ui::InputType::KeyDown;
    escape.key = ui::Key::Escape;
    NUI_CHECK(ui::handled(keys.dispatch(escape, platform)));
    NUI_CHECK(!lower_escape_handle.valid());
    NUI_CHECK(visual_only_handle.valid());
    NUI_CHECK(root_keys->escape_downs == 0);

    keys.resize({96.0f, 48.0f});
    NUI_CHECK(ui::handled(keys.dispatch(escape, platform)));
    NUI_CHECK(root_keys->escape_downs == 1);
    NUI_CHECK(keys.close_overlay(visual_only_handle));

    auto protected_lower = centered_overlay(
        ui::make_spec(OverlayProbe{std::make_shared<OverlayProbeState>()}));
    protected_lower.dismiss_on_escape = true;
    const auto protected_handle = keys.show_overlay(std::move(protected_lower));

    auto blocking_modal = centered_overlay(
        ui::make_spec(OverlayProbe{std::make_shared<OverlayProbeState>()}));
    blocking_modal.mode = ui::OverlayMode::Modal;
    const auto modal_handle = keys.show_overlay(std::move(blocking_modal));
    keys.resize({96.0f, 48.0f});

    NUI_CHECK(ui::handled(keys.dispatch(escape, platform)));
    NUI_CHECK(protected_handle.valid());
    NUI_CHECK(modal_handle.valid());
    NUI_CHECK(root_keys->escape_downs == 1);

    NUI_CHECK(keys.close_overlay(modal_handle));
    keys.resize({96.0f, 48.0f});
    NUI_CHECK(ui::handled(keys.dispatch(escape, platform)));
    NUI_CHECK(!protected_handle.valid());
    NUI_CHECK(root_keys->escape_downs == 1);
}

void overlay_structural_queue_contract() {
    test::MockPlatform platform;
    auto state = std::make_shared<OverlayProbeState>();

    ui::UI tree{
        ui::Canvas{ui::Size{96.0f, 48.0f}, [](ui::CanvasContext2D&) {}}
    };
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    const auto handle = tree.show_overlay(
        centered_overlay(ui::make_spec(OverlayProbe{state})));
    NUI_CHECK(handle.valid());
    NUI_CHECK(state->mounts == 0);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(state->mounts == 1);
    NUI_CHECK(state->activates == 1);

    NUI_CHECK(tree.close_overlay(handle));
    NUI_CHECK(!handle.valid());
    NUI_CHECK(state->unmounts == 0);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(state->deactivates == 1);
    NUI_CHECK(state->unmounts == 1);
    NUI_CHECK(!tree.close_overlay(handle));

    auto coalesced = std::make_shared<OverlayProbeState>();
    const auto transient = tree.show_overlay(
        centered_overlay(ui::make_spec(OverlayProbe{coalesced})));
    NUI_CHECK(tree.close_overlay(transient));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(coalesced->mounts == 0);
    NUI_CHECK(coalesced->unmounts == 0);

    ui::UI other{
        ui::Canvas{ui::Size{96.0f, 48.0f}, [](ui::CanvasContext2D&) {}}
    };
    const auto owned = tree.show_overlay(centered_overlay(ui::make_spec(OverlayProbe{coalesced})));
    NUI_CHECK(!other.close_overlay(owned));
    NUI_CHECK(tree.close_overlay(owned));

    auto invalid = centered_overlay(ui::make_spec(OverlayProbe{coalesced}));
    invalid.mode = ui::OverlayMode::Modal;
    invalid.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    NUI_CHECK(!tree.show_overlay(std::move(invalid)).valid());
}

void suite() {
    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};

    ui::UI seed{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D& canvas) {
                canvas.fill_rect(
                    ui::Rect{0.0f, 0.0f, 96.0f, 48.0f},
                    ui::Color{0.15f, 0.35f, 0.65f, 1.0f});
            }}
    };
    NUI_CHECK(renderer.render(seed));
    const auto seeded_pixels = renderer.rgba_pixels();

    ui::UI empty{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D&) {}}
    };
    NUI_CHECK(renderer.render(empty));
    NUI_CHECK(renderer.rgba_pixels() != seeded_pixels);

    bool only_renderer_clear = true;
    const auto& empty_pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i < empty_pixels.size(); i += 4) {
        if (empty_pixels[i] != 0 || empty_pixels[i + 1] != 0 ||
            empty_pixels[i + 2] != 0 || empty_pixels[i + 3] != 255) {
            only_renderer_clear = false;
            break;
        }
    }
    NUI_CHECK(only_renderer_clear);

    ui::State<bool> enabled{true};
    ui::UI tree{
        ui::Padding{4.0f,
            ui::Toggle{"Enabled", enabled}}
    };

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 96);
    NUI_CHECK(renderer.pixel_height() == 48);
    NUI_CHECK(renderer.rgba_pixels().size() == 96U * 48U * 4U);

    renderer.resize({96.0f, 48.0f}, 2.0f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 192);
    NUI_CHECK(renderer.pixel_height() == 96);
    NUI_CHECK(renderer.rgba_pixels().size() == 192U * 96U * 4U);

    enabled.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    overlay_placement_contract();
    overlay_pointer_stack_contract();
    overlay_modal_pointer_barrier_contract();
    overlay_dismissal_contract();
    overlay_structural_queue_contract();
}

} // namespace

int main() { return test::run("headless", &suite); }
