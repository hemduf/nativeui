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

struct FocusProbeState {
    int gains{};
    int losses{};
    bool focused{};
};

class FocusProbeComponent final : public ui::Component {
public:
    FocusProbeComponent(ui::Size size, std::shared_ptr<FocusProbeState> state)
        : size_(size), state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return size_;
    }

    void focus_changed(bool focused, ui::FocusContext&) override {
        state_->focused = focused;
        if (focused) ++state_->gains;
        else ++state_->losses;
    }

    void paint(ui::PaintContext&) const override {}

private:
    ui::Size size_{};
    std::shared_ptr<FocusProbeState> state_;
};

class FocusProbe {
public:
    FocusProbe(ui::Size size, std::shared_ptr<FocusProbeState> state)
        : size_(size), state_(std::move(state)) {}

    ui::Spec spec() && {
        const auto size = size_;
        auto state = std::move(state_);
        return ui::Spec{
            [size, state = std::move(state)] {
                return std::make_unique<FocusProbeComponent>(size, state);
            },
            {}};
    }

private:
    ui::Size size_{};
    std::shared_ptr<FocusProbeState> state_;
};

struct CaptureCloseState {
    int pointer_downs{};
    int pointer_cancels{};
    int unmounts{};
    bool in_pointer_down{};
    bool destroyed_reentrantly{};
    bool close_result{};
    std::function<void()> close;
};

class CaptureCloseComponent final : public ui::Component {
public:
    explicit CaptureCloseComponent(std::shared_ptr<CaptureCloseState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {24.0f, 16.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerCancel) {
            ++state_->pointer_cancels;
            return ui::EventResult::Handled;
        }
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;

        ++state_->pointer_downs;
        state_->in_pointer_down = true;
        context.capture_pointer();
        if (state_->close) state_->close();
        if (state_->unmounts != 0) state_->destroyed_reentrantly = true;
        state_->in_pointer_down = false;
        return ui::EventResult::Handled;
    }

    void unmount(ui::LifecycleContext&) override {
        if (state_->in_pointer_down) state_->destroyed_reentrantly = true;
        ++state_->unmounts;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<CaptureCloseState> state_;
};

class CaptureCloseProbe {
public:
    explicit CaptureCloseProbe(std::shared_ptr<CaptureCloseState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<CaptureCloseComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<CaptureCloseState> state_;
};

struct AnchorProbeState {
    ui::NodeId id{ui::kInvalidNodeId};
};

class AnchorProbeComponent final : public ui::Component {
public:
    explicit AnchorProbeComponent(std::shared_ptr<AnchorProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 10.0f};
    }

    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<AnchorProbeState> state_;
};

class AnchorProbe {
public:
    explicit AnchorProbe(std::shared_ptr<AnchorProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<AnchorProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<AnchorProbeState> state_;
};

class AnchorRootComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) {
            placements.front().bounds = {
                bounds.x + (std::max)(0.0f, bounds.w - 30.0f),
                bounds.y + 8.0f,
                20.0f,
                10.0f};
        }
    }

    void paint(ui::PaintContext&) const override {}
};

class AnchorRoot {
public:
    AnchorRoot(ui::State<bool>& present, std::shared_ptr<AnchorProbeState> state)
        : present_(&present), state_(std::move(state)) {}

    ui::Spec spec() && {
        auto* present = present_;
        auto state = std::move(state_);
        return ui::Spec{
            [] { return std::make_unique<AnchorRootComponent>(); },
            {ui::make_spec(ui::If{*present, AnchorProbe{std::move(state)}})}};
    }

private:
    ui::State<bool>* present_{};
    std::shared_ptr<AnchorProbeState> state_;
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

void overlay_anchor_contract() {
    test::MockPlatform platform;
    ui::State<bool> anchor_present{true};
    auto anchor = std::make_shared<AnchorProbeState>();
    auto overlay = std::make_shared<PointerProbeState>();

    ui::UI tree{AnchorRoot{anchor_present, anchor}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);
    NUI_CHECK(anchor->id != ui::kInvalidNodeId);

    auto anchored = centered_pointer_overlay(overlay);
    anchored.anchor = anchor->id;
    anchored.placement = ui::OverlayPlacement::AnchorBelow;
    const auto handle = tree.show_overlay(std::move(anchored));
    tree.resize({96.0f, 48.0f});

    ui::InputEvent first;
    first.type = ui::InputType::PointerDown;
    first.position = {68.0f, 20.0f};
    NUI_CHECK(ui::handled(tree.dispatch(first, platform)));
    NUI_CHECK(overlay->pointer_downs == 1);

    tree.resize({120.0f, 48.0f});
    ui::InputEvent moved = first;
    moved.position = {92.0f, 20.0f};
    NUI_CHECK(ui::handled(tree.dispatch(moved, platform)));
    NUI_CHECK(overlay->pointer_downs == 2);

    anchor_present.set(false);
    tree.resize({120.0f, 48.0f});
    NUI_CHECK(!handle.valid());

    ui::State<bool> second_present{true};
    auto second_anchor = std::make_shared<AnchorProbeState>();
    ui::UI second{AnchorRoot{second_present, second_anchor}};
    second.resize({96.0f, 48.0f});
    second.activate(platform);
    auto second_spec = centered_overlay(
        ui::make_spec(OverlayProbe{std::make_shared<OverlayProbeState>()}));
    second_spec.anchor = second_anchor->id;
    second_spec.placement = ui::OverlayPlacement::AnchorBelow;
    const auto second_handle = second.show_overlay(std::move(second_spec));
    second.resize({96.0f, 48.0f});
    NUI_CHECK(second_handle.valid());
    second.deactivate(platform);
    NUI_CHECK(!second_handle.valid());
}

void overlay_focus_and_capture_contract() {
    test::MockPlatform platform;
    auto root_focus = std::make_shared<FocusProbeState>();
    auto first_focus = std::make_shared<FocusProbeState>();
    auto second_focus = std::make_shared<FocusProbeState>();

    ui::UI tree{FocusProbe{{96.0f, 48.0f}, root_focus}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);
    NUI_CHECK(root_focus->focused);

    auto first_modal = centered_overlay(ui::make_spec(FocusProbe{{24.0f, 16.0f}, first_focus}));
    first_modal.mode = ui::OverlayMode::Modal;
    const auto first_handle = tree.show_overlay(std::move(first_modal));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(!root_focus->focused);
    NUI_CHECK(first_focus->focused);

    auto second_modal = centered_overlay(ui::make_spec(FocusProbe{{24.0f, 16.0f}, second_focus}));
    second_modal.mode = ui::OverlayMode::Modal;
    const auto second_handle = tree.show_overlay(std::move(second_modal));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(!first_focus->focused);
    NUI_CHECK(second_focus->focused);

    NUI_CHECK(tree.close_overlay(second_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(!root_focus->focused);
    NUI_CHECK(first_focus->focused);
    NUI_CHECK(!second_focus->focused);

    NUI_CHECK(tree.close_overlay(first_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(root_focus->focused);
    NUI_CHECK(!first_focus->focused);

    auto capture = std::make_shared<CaptureCloseState>();
    ui::UI capture_tree{
        ui::Canvas{ui::Size{96.0f, 48.0f}, [](ui::CanvasContext2D&) {}}
    };
    capture_tree.resize({96.0f, 48.0f});
    capture_tree.activate(platform);

    auto capture_spec = centered_overlay(ui::make_spec(CaptureCloseProbe{capture}));
    const auto capture_handle = capture_tree.show_overlay(std::move(capture_spec));
    capture_tree.resize({96.0f, 48.0f});
    capture->close = [&capture_tree, capture_handle, capture] {
        capture->close_result = capture_tree.close_overlay(capture_handle);
    };

    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {48.0f, 24.0f};
    NUI_CHECK(ui::handled(capture_tree.dispatch(down, platform)));
    NUI_CHECK(capture->pointer_downs == 1);
    NUI_CHECK(capture->close_result);
    NUI_CHECK(!capture_handle.valid());
    NUI_CHECK(capture->pointer_cancels == 1);
    NUI_CHECK(capture->unmounts == 1);
    NUI_CHECK(!capture->destroyed_reentrantly);
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
    overlay_anchor_contract();
    overlay_focus_and_capture_contract();
    overlay_structural_queue_contract();
}

} // namespace

int main() { return test::run("headless", &suite); }
