#include "test_support.hpp"

namespace {

struct AnchorState {
    ui::NodeId id{ui::kInvalidNodeId};
};

class AnchorComponent final : public ui::Component {
public:
    explicit AnchorComponent(std::shared_ptr<AnchorState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 10.0f};
    }

    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<AnchorState> state_;
};

class Anchor {
public:
    explicit Anchor(std::shared_ptr<AnchorState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<AnchorComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<AnchorState> state_;
};

class RootComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        for (std::size_t i = 0; i < placements.size(); ++i) {
            placements[i].bounds = {
                bounds.x + 8.0f + static_cast<float>(i) * 28.0f,
                bounds.y + 8.0f,
                20.0f,
                10.0f};
        }
    }

    void paint(ui::PaintContext&) const override {}
};

class AvailabilityAnchorRoot {
public:
    AvailabilityAnchorRoot(ui::State<ui::VisibilityMode>& visibility,
                           std::shared_ptr<AnchorState> state)
        : visibility_(&visibility), state_(std::move(state)) {}

    ui::Spec spec() && {
        auto* visibility = visibility_;
        auto state = std::move(state_);
        return ui::Spec{
            [] { return std::make_unique<RootComponent>(); },
            {ui::make_spec(ui::Visibility{*visibility, Anchor{std::move(state)}})}};
    }

private:
    ui::State<ui::VisibilityMode>* visibility_{};
    std::shared_ptr<AnchorState> state_;
};

struct FocusState {
    bool focused{};
};

class FocusComponent final : public ui::Component {
public:
    explicit FocusComponent(std::shared_ptr<FocusState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 10.0f};
    }

    void focus_changed(bool focused, ui::FocusContext&) override {
        state_->focused = focused;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<FocusState> state_;
};

class FocusProbe {
public:
    explicit FocusProbe(std::shared_ptr<FocusState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<FocusComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<FocusState> state_;
};

class FocusRoot {
public:
    FocusRoot(ui::State<ui::VisibilityMode>& first_visibility,
              std::shared_ptr<FocusState> first,
              std::shared_ptr<FocusState> second)
        : first_visibility_(&first_visibility),
          first_(std::move(first)),
          second_(std::move(second)) {}

    ui::Spec spec() && {
        auto* visibility = first_visibility_;
        auto first = std::move(first_);
        auto second = std::move(second_);
        return ui::Spec{
            [] { return std::make_unique<RootComponent>(); },
            {
                ui::make_spec(ui::Visibility{*visibility, FocusProbe{std::move(first)}}),
                ui::make_spec(FocusProbe{std::move(second)}),
            }};
    }

private:
    ui::State<ui::VisibilityMode>* first_visibility_{};
    std::shared_ptr<FocusState> first_;
    std::shared_ptr<FocusState> second_;
};

struct MountState {
    int mounts{};
    int unmounts{};
};

class MountProbeComponent final : public ui::Component {
public:
    explicit MountProbeComponent(std::shared_ptr<MountState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {24.0f, 16.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<MountState> state_;
};

class MountProbe {
public:
    explicit MountProbe(std::shared_ptr<MountState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<MountProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<MountState> state_;
};

struct ReentrantShowState {
    int pointer_downs{};
    bool mounted_reentrantly{};
    ui::OverlayHandle handle;
    std::function<ui::OverlayHandle()> show;
    std::shared_ptr<MountState> overlay_lifecycle;
};

class ReentrantShowComponent final : public ui::Component {
public:
    explicit ReentrantShowComponent(std::shared_ptr<ReentrantShowState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;

        ++state_->pointer_downs;
        state_->handle = state_->show();
        state_->mounted_reentrantly = state_->overlay_lifecycle->mounts != 0;
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ReentrantShowState> state_;
};

class ReentrantShowProbe {
public:
    explicit ReentrantShowProbe(std::shared_ptr<ReentrantShowState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ReentrantShowComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ReentrantShowState> state_;
};

ui::OverlaySpec centered(ui::Spec content) {
    ui::OverlaySpec overlay;
    overlay.placement = ui::OverlayPlacement::Center;
    overlay.content = std::move(content);
    return overlay;
}

void anchor_visibility_contract() {
    test::MockPlatform platform;

    for (const auto unavailable : {ui::VisibilityMode::Hidden,
                                   ui::VisibilityMode::Collapsed}) {
        ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
        auto anchor = std::make_shared<AnchorState>();
        ui::UI tree{AvailabilityAnchorRoot{visibility, anchor}};
        tree.resize({96.0f, 48.0f});
        tree.activate(platform);
        NUI_CHECK(anchor->id != ui::kInvalidNodeId);

        auto overlay = centered(ui::make_spec(FocusProbe{std::make_shared<FocusState>()}));
        overlay.anchor = anchor->id;
        overlay.placement = ui::OverlayPlacement::AnchorBelow;
        const auto handle = tree.show_overlay(std::move(overlay));
        tree.resize({96.0f, 48.0f});
        NUI_CHECK(handle.valid());

        visibility.set(unavailable);

        ui::InputEvent pointer;
        pointer.type = ui::InputType::PointerDown;
        pointer.position = {4.0f, 4.0f};
        (void)tree.dispatch(pointer, platform);
        NUI_CHECK(!handle.valid());
    }
}

void stale_focus_restoration_contract() {
    test::MockPlatform platform;
    ui::State<ui::VisibilityMode> first_visibility{ui::VisibilityMode::Visible};
    auto first = std::make_shared<FocusState>();
    auto second = std::make_shared<FocusState>();
    auto modal_focus = std::make_shared<FocusState>();

    ui::UI tree{FocusRoot{first_visibility, first, second}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);
    NUI_CHECK(first->focused);
    NUI_CHECK(!second->focused);

    auto modal = centered(ui::make_spec(FocusProbe{modal_focus}));
    modal.mode = ui::OverlayMode::Modal;
    const auto handle = tree.show_overlay(std::move(modal));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(modal_focus->focused);
    NUI_CHECK(!first->focused);

    first_visibility.set(ui::VisibilityMode::Hidden);
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(modal_focus->focused);

    NUI_CHECK(tree.close_overlay(handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(!modal_focus->focused);
    NUI_CHECK(!first->focused);
    NUI_CHECK(second->focused);
}

void pointer_transparent_focus_contract() {
    test::MockPlatform platform;
    auto root_focus = std::make_shared<FocusState>();
    auto overlay_focus = std::make_shared<FocusState>();

    ui::UI tree{FocusProbe{root_focus}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);
    NUI_CHECK(root_focus->focused);

    auto overlay = centered(ui::make_spec(FocusProbe{overlay_focus}));
    overlay.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    const auto handle = tree.show_overlay(std::move(overlay));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(handle.valid());
    NUI_CHECK(root_focus->focused);
    NUI_CHECK(!overlay_focus->focused);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(root_focus->focused);
    NUI_CHECK(!overlay_focus->focused);

    NUI_CHECK(tree.close_overlay(handle));
}

void per_ui_handle_isolation_contract() {
    test::MockPlatform platform;
    auto a_root = std::make_shared<FocusState>();
    auto b_root = std::make_shared<FocusState>();

    ui::UI a{FocusProbe{a_root}};
    ui::UI b{FocusProbe{b_root}};
    a.resize({96.0f, 48.0f});
    b.resize({96.0f, 48.0f});
    a.activate(platform);
    b.activate(platform);

    const auto a_first = a.show_overlay(centered(
        ui::make_spec(FocusProbe{std::make_shared<FocusState>()})));
    const auto b_first = b.show_overlay(centered(
        ui::make_spec(FocusProbe{std::make_shared<FocusState>()})));
    a.resize({96.0f, 48.0f});
    b.resize({96.0f, 48.0f});

    NUI_CHECK(a_first.valid());
    NUI_CHECK(b_first.valid());
    NUI_CHECK(!a.close_overlay(b_first));
    NUI_CHECK(!b.close_overlay(a_first));

    NUI_CHECK(a.close_overlay(a_first));
    a.resize({96.0f, 48.0f});
    NUI_CHECK(!a_first.valid());
    NUI_CHECK(b_first.valid());

    const auto a_second = a.show_overlay(centered(
        ui::make_spec(FocusProbe{std::make_shared<FocusState>()})));
    NUI_CHECK(a_second.valid());
    NUI_CHECK(a_second != a_first);
    NUI_CHECK(b.close_overlay(b_first));
    NUI_CHECK(a.close_overlay(a_second));
}

void reentrant_show_contract() {
    test::MockPlatform platform;
    auto lifecycle = std::make_shared<MountState>();
    auto state = std::make_shared<ReentrantShowState>();
    state->overlay_lifecycle = lifecycle;

    ui::UI tree{ReentrantShowProbe{state}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    state->show = [&tree, lifecycle] {
        return tree.show_overlay(centered(ui::make_spec(MountProbe{lifecycle})));
    };

    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {8.0f, 8.0f};
    NUI_CHECK(ui::handled(tree.dispatch(down, platform)));
    NUI_CHECK(state->pointer_downs == 1);
    NUI_CHECK(state->handle.valid());
    NUI_CHECK(lifecycle->mounts == 0);
    NUI_CHECK(!state->mounted_reentrantly);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->mounts == 1);

    NUI_CHECK(tree.close_overlay(state->handle));
    NUI_CHECK(!state->handle.valid());
    NUI_CHECK(lifecycle->unmounts == 0);
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->unmounts == 1);
}

void suite() {
    anchor_visibility_contract();
    stale_focus_restoration_contract();
    pointer_transparent_focus_contract();
    per_ui_handle_isolation_contract();
    reentrant_show_contract();
}

} // namespace

int main() { return test::run("t061_overlay_acceptance", &suite); }
