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

void suite() {
    anchor_visibility_contract();
    stale_focus_restoration_contract();
    per_ui_handle_isolation_contract();
}

} // namespace

int main() { return test::run("t061_overlay_acceptance", &suite); }
