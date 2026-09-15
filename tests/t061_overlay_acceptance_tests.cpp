#include "test_support.hpp"

#include <functional>
#include <stdexcept>

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

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

ui::DialogSpec dismissal_dialog_spec() {
    ui::DialogSpec spec;
    spec.body = ui::make_spec(ui::Spacer{80.0f, 40.0f});
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});
    return spec;
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
    NUI_CHECK(!state->mounted_reentrantly);
    // T058 owns the safe structural checkpoint at top-level dispatch exit:
    // mounting after the callback has unwound is expected and must not be
    // delayed to a second overlay-specific queue/checkpoint.
    NUI_CHECK(lifecycle->mounts == 1);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->mounts == 1);

    NUI_CHECK(tree.close_overlay(state->handle));
    NUI_CHECK(!state->handle.valid());
    NUI_CHECK(lifecycle->unmounts == 0);
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->unmounts == 1);
}

void middle_removal_contract() {
    test::MockPlatform platform;
    auto first = std::make_shared<MountState>();
    auto middle = std::make_shared<MountState>();
    auto last = std::make_shared<MountState>();

    ui::UI tree{ui::Spacer{96.0f, 48.0f}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    const auto first_handle =
        tree.show_overlay(centered(ui::make_spec(MountProbe{first})));
    const auto middle_handle =
        tree.show_overlay(centered(ui::make_spec(MountProbe{middle})));
    const auto last_handle =
        tree.show_overlay(centered(ui::make_spec(MountProbe{last})));
    tree.resize({96.0f, 48.0f});

    NUI_CHECK(first->mounts == 1);
    NUI_CHECK(middle->mounts == 1);
    NUI_CHECK(last->mounts == 1);

    NUI_CHECK(tree.close_overlay(middle_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(middle->unmounts == 1);
    NUI_CHECK(first->mounts == 1 && first->unmounts == 0);
    NUI_CHECK(last->mounts == 1 && last->unmounts == 0);
    NUI_CHECK(first_handle.valid());
    NUI_CHECK(last_handle.valid());

    NUI_CHECK(tree.close_overlay(last_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(last->unmounts == 1);
    NUI_CHECK(first->mounts == 1 && first->unmounts == 0);
    NUI_CHECK(first_handle.valid());

    NUI_CHECK(tree.close_overlay(first_handle));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(first->unmounts == 1);
}

void throwing_show_is_transactional_contract() {
    auto state = std::make_shared<ui::detail::OverlayState>();
    ui::detail::OverlayHostComponent host{state};
    state->structural_invalidator = [] { throw std::runtime_error{"show invalidation"}; };

    auto modal = centered(ui::make_spec(ui::Spacer{24.0f, 16.0f}));
    modal.mode = ui::OverlayMode::Modal;
    bool threw = false;
    try {
        (void)state->show(std::move(modal));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    NUI_CHECK(threw);
    NUI_CHECK(state->entries.empty());
    NUI_CHECK(state->next_id == 2);
    const auto failed_keys = host.desired_keys();
    NUI_CHECK(failed_keys.size() == 1);
    NUI_CHECK(failed_keys.front() == "root");

    state->structural_invalidator = [] {};
    auto recovered_modal = centered(ui::make_spec(ui::Spacer{24.0f, 16.0f}));
    recovered_modal.mode = ui::OverlayMode::Modal;
    const auto recovered = state->show(std::move(recovered_modal));
    NUI_CHECK(recovered.valid());
    NUI_CHECK(state->entries.size() == 1);
    NUI_CHECK(state->entries.front().id == 2);
    const auto recovered_keys = host.desired_keys();
    NUI_CHECK(recovered_keys.size() == 3);
    NUI_CHECK(recovered_keys[1] == "modal-barrier:2");
    NUI_CHECK(recovered_keys[2] == "overlay:2");
    NUI_CHECK(state->close(recovered));
}

void throwing_close_is_transactional_contract() {
    auto state = std::make_shared<ui::detail::OverlayState>();
    ui::detail::OverlayHostComponent host{state};
    state->structural_invalidator = [] {};

    auto first_overlay = centered(ui::make_spec(ui::Spacer{24.0f, 16.0f}));
    first_overlay.mode = ui::OverlayMode::Modal;
    const auto first = state->show(std::move(first_overlay));
    NUI_CHECK(first.valid());
    NUI_CHECK(state->entries.size() == 1);

    state->structural_invalidator = [] { throw std::runtime_error{"close invalidation"}; };
    bool handle_close_threw = false;
    try {
        (void)state->close(first);
    } catch (const std::runtime_error&) {
        handle_close_threw = true;
    }
    NUI_CHECK(handle_close_threw);
    NUI_CHECK(first.valid());
    NUI_CHECK(state->entries.size() == 1);
    NUI_CHECK(host.desired_keys().size() == 3);

    state->structural_invalidator = [] {};
    NUI_CHECK(state->close(first));
    NUI_CHECK(!first.valid());
    NUI_CHECK(!state->close(first));
    NUI_CHECK(state->entries.empty());

    const auto second = state->show(centered(ui::make_spec(ui::Spacer{24.0f, 16.0f})));
    NUI_CHECK(second.valid());
    NUI_CHECK(state->entries.size() == 1);
    const auto second_id = state->entries.front().id;

    state->structural_invalidator = [] { throw std::runtime_error{"close id invalidation"}; };
    bool id_close_threw = false;
    try {
        (void)state->close_id(second_id);
    } catch (const std::runtime_error&) {
        id_close_threw = true;
    }
    NUI_CHECK(id_close_threw);
    NUI_CHECK(second.valid());
    NUI_CHECK(state->entries.size() == 1);

    state->structural_invalidator = [] {};
    NUI_CHECK(state->close_id(second_id));
    NUI_CHECK(!second.valid());
    NUI_CHECK(!state->close_id(second_id));
}

void throwing_overlay_state_is_per_ui_contract() {
    auto failing = std::make_shared<ui::detail::OverlayState>();
    auto healthy = std::make_shared<ui::detail::OverlayState>();
    failing->structural_invalidator = [] { throw std::runtime_error{"isolated invalidation"}; };
    healthy->structural_invalidator = [] {};

    bool threw = false;
    try {
        (void)failing->show(centered(ui::make_spec(ui::Spacer{8.0f, 8.0f})));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(failing->entries.empty());

    const auto healthy_handle =
        healthy->show(centered(ui::make_spec(ui::Spacer{8.0f, 8.0f})));
    NUI_CHECK(healthy_handle.valid());
    NUI_CHECK(healthy->entries.size() == 1);
    NUI_CHECK(healthy->close(healthy_handle));
}

void dialog_completion_may_destroy_controller_contract() {
    test::MockPlatform platform;
    ui::UI tree{ui::Spacer{160.0f, 96.0f}};
    tree.resize({160.0f, 96.0f});
    tree.activate(platform);

    std::unique_ptr<ui::Dialog> dialog = std::make_unique<ui::Dialog>(tree);
    int completions = 0;
    NUI_CHECK(dialog->show(dismissal_dialog_spec(), [&](ui::DialogResult result) {
        NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
        ++completions;
        dialog.reset();
    }) == ui::DialogShowResult::Shown);
    tree.resize({160.0f, 96.0f});

    auto* close_target = dialog.get();
    NUI_CHECK(close_target->close());
    NUI_CHECK(!dialog);
    NUI_CHECK(completions == 1);
    NUI_CHECK(tree.overlay_entries().empty());

    ui::Dialog recovered{tree};
    int recovered_completions = 0;
    NUI_CHECK(recovered.show(dismissal_dialog_spec(), [&](ui::DialogResult) {
        ++recovered_completions;
    }) == ui::DialogShowResult::Shown);
    tree.resize({160.0f, 96.0f});
    NUI_CHECK(recovered.close());
    NUI_CHECK(recovered_completions == 1);
    NUI_CHECK(!recovered.active());
}

void dialog_completion_may_destroy_ui_and_throw_contract() {
    test::MockPlatform platform;
    auto tree = std::make_unique<ui::UI>(ui::Spacer{160.0f, 96.0f});
    tree->resize({160.0f, 96.0f});
    tree->activate(platform);

    ui::Dialog dialog{*tree};
    int completions = 0;
    NUI_CHECK(dialog.show(dismissal_dialog_spec(), [&](ui::DialogResult result) {
        NUI_CHECK(result.kind == ui::DialogResultKind::Action);
        NUI_CHECK(result.action_id == "confirm");
        ++completions;
        tree.reset();
        throw std::runtime_error{"completion destroyed UI"};
    }) == ui::DialogShowResult::Shown);
    tree->resize({160.0f, 96.0f});

    auto* dispatch_target = tree.get();
    bool threw = false;
    try {
        (void)dispatch_target->dispatch(test::key(ui::Key::Enter), platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(!tree);
    NUI_CHECK(completions == 1);
    NUI_CHECK(!dialog.active());
}

void popup_action_may_destroy_ui_contract() {
    test::MockPlatform platform;
    std::unique_ptr<ui::UI> tree;
    int actions = 0;
    tree = std::make_unique<ui::UI>(ui::PopupMenu{
        "Actions",
        {ui::PopupMenuItem::action("Run", [&] {
            ++actions;
            tree.reset();
        })}});
    tree->resize({160.0f, 96.0f});
    tree->activate(platform);

    (void)tree->dispatch(test::key(ui::Key::Enter), platform);
    (void)tree->dispatch(key_up(ui::Key::Enter), platform);
    NUI_CHECK(tree);
    NUI_CHECK(tree->overlay_entries().size() == 1);

    auto* dispatch_target = tree.get();
    const auto result = dispatch_target->dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(result == ui::EventResult::Handled);
    NUI_CHECK(!tree);
    NUI_CHECK(actions == 1);
}

void suite() {
    anchor_visibility_contract();
    stale_focus_restoration_contract();
    pointer_transparent_focus_contract();
    per_ui_handle_isolation_contract();
    reentrant_show_contract();
    middle_removal_contract();
    throwing_show_is_transactional_contract();
    throwing_close_is_transactional_contract();
    throwing_overlay_state_is_per_ui_contract();
    dialog_completion_may_destroy_controller_contract();
    dialog_completion_may_destroy_ui_and_throw_contract();
    popup_action_may_destroy_ui_contract();
}

} // namespace

int main() { return test::run("t061_overlay_acceptance", &suite); }
