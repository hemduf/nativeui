#include "test_support.hpp"

#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/overlay_commands.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

class FixedRootComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {240.0f, 220.0f};
    }

    void layout_children(
        ui::Rect,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements[0].bounds = {20.0f, 20.0f, 120.0f, 40.0f};
        if (placements.size() > 1) placements[1].bounds = {20.0f, 160.0f, 120.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

class FixedRoot {
public:
    explicit FixedRoot(std::vector<ui::Spec> children)
        : children_(std::move(children)) {}

    ui::Spec spec() && {
        return ui::Spec{
            [] { return std::make_unique<FixedRootComponent>(); },
            std::move(children_)};
    }

private:
    std::vector<ui::Spec> children_;
};

struct ThemeProbeState {
    float mounted_control_height{-1.0f};
};

class ThemeProbeComponent final : public ui::Component,
                                  public ui::detail::ThemeBinding {
public:
    explicit ThemeProbeComponent(std::shared_ptr<ThemeProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {10.0f, 10.0f};
    }

    void mount(ui::MountContext&) override {
        state_->mounted_control_height = current_theme().controls.control_height;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ThemeProbeState> state_;
};

class ThemeProbe {
public:
    explicit ThemeProbe(std::shared_ptr<ThemeProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)]() mutable {
                return std::make_unique<ThemeProbeComponent>(std::move(state));
            },
            {}};
    }

private:
    std::shared_ptr<ThemeProbeState> state_;
};

void combo_keyboard_commit_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{2};
    int writes = 0;
    auto observer = selection.observe([&](const int&) { ++writes; });

    ui::UI tree{ui::ComboBox<int>{
        selection,
        {{1, "One", true}, {2, "Two", true}, {3, "Three", false}}}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(selection.get() == 2);
    NUI_CHECK(writes == 0);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(selection.get() == 2);
    NUI_CHECK(writes == 0);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(selection.get() == 2);
    NUI_CHECK(writes == 0);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(writes == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
}

void no_match_home_end_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{99};
    int writes = 0;
    auto observer = selection.observe([&](const int&) { ++writes; });

    ui::UI tree{ui::ComboBox<int>{
        selection,
        {{1, "One", true}, {2, "Two", false}, {3, "Three", true}}}
                    .placeholder("Choose")};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(selection.get() == 99);
    NUI_CHECK(writes == 0);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 99);
    NUI_CHECK(writes == 0);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::End), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 3);
    NUI_CHECK(writes == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Home), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Space), platform)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(writes == 2);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Space), platform)));
}

void immutable_open_snapshot_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{99};
    std::vector<ui::ComboBoxOption<int>> model{{1, "One", true}, {2, "Two", true}};
    int provider_calls = 0;

    ui::UI tree{ui::ComboBox<int>{selection, [&] {
        ++provider_calls;
        return model;
    }}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(provider_calls == 1);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(provider_calls == 2);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));

    model = {{7, "Seven", true}};
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::End), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 2);
    NUI_CHECK(provider_calls == 2);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(provider_calls == 3);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 7);
}

void provider_availability_reentrancy_contract() {
    {
        test::MockPlatform platform;
        ui::State<int> selection{1};
        ui::State<bool> enabled{true};
        auto after = std::make_shared<test::ProbeState>();
        after->input_result = ui::EventResult::Handled;
        int provider_calls = 0;

        ui::UI tree{ui::Column{
            ui::Enabled{
                enabled,
                ui::ComboBox<int>{selection, [&] {
                    ++provider_calls;
                    if (provider_calls == 2) enabled.set(false);
                    return std::vector<ui::ComboBoxOption<int>>{
                        {1, "One", true}, {2, "Two", true}};
                }}},
            test::Probe{after},
        }};
        tree.resize({240.0f, 180.0f});
        tree.activate(platform);

        NUI_CHECK(provider_calls == 1);
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
        NUI_CHECK(provider_calls == 2);
        NUI_CHECK(!enabled.get());
        NUI_CHECK(selection.get() == 1);
        NUI_CHECK(after->focus_in >= 1);

        after->key_events = 0;
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
        NUI_CHECK(after->key_events == 1);
        NUI_CHECK(selection.get() == 1);

        enabled.set(true);
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Tab, true), platform)));
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
        NUI_CHECK(provider_calls == 3);
        NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
        NUI_CHECK(selection.get() == 2);
    }

    {
        test::MockPlatform platform;
        ui::State<int> selection{1};
        ui::State<bool> read_only{false};
        int provider_calls = 0;

        ui::UI tree{ui::ReadOnly{
            read_only,
            ui::ComboBox<int>{selection, [&] {
                ++provider_calls;
                if (provider_calls == 2) read_only.set(true);
                return std::vector<ui::ComboBoxOption<int>>{
                    {1, "One", true}, {2, "Two", true}};
            }}}};
        tree.resize({240.0f, 180.0f});
        tree.activate(platform);

        NUI_CHECK(provider_calls == 1);
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
        NUI_CHECK(provider_calls == 2);
        NUI_CHECK(read_only.get());
        NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Escape), platform) == ui::EventResult::Ignored);
        NUI_CHECK(selection.get() == 1);
    }
}

void tab_escape_and_read_only_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::State<bool> read_only{false};
    auto after = std::make_shared<test::ProbeState>();

    ui::UI tree{ui::Column{
        ui::ReadOnly{read_only,
                     ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}},
        test::Probe{after},
    }};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Tab), platform)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(after->focus_in == 1);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Tab, true), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Escape), platform)));
    NUI_CHECK(selection.get() == 1);

    read_only.set(true);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Space), platform)));

    auto alt_down = test::key(ui::Key::Down);
    alt_down.alt = true;
    NUI_CHECK(ui::handled(tree.dispatch(alt_down, platform)));

    // Column has 24 px default padding, so use a point inside the 40 px anchor.
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 30.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 30.0f), platform)));
    NUI_CHECK(selection.get() == 1);

    read_only.set(false);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    read_only.set(true);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 1);
}

void disabled_and_destroyed_anchor_close_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::State<bool> enabled{true};
    ui::State<bool> present{true};

    ui::UI disabled_tree{ui::Enabled{
        enabled,
        ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}}};
    disabled_tree.resize({240.0f, 180.0f});
    disabled_tree.activate(platform);
    NUI_CHECK(ui::handled(disabled_tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(disabled_tree.dispatch(key_up(ui::Key::Down), platform)));
    enabled.set(false);
    (void)disabled_tree.dispatch(test::key(ui::Key::Enter), platform);
    NUI_CHECK(selection.get() == 1);

    test::MockPlatform platform2;
    ui::UI destroyed_tree{ui::If{
        present,
        ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}}};
    destroyed_tree.resize({240.0f, 180.0f});
    destroyed_tree.activate(platform2);
    NUI_CHECK(ui::handled(destroyed_tree.dispatch(test::key(ui::Key::Down), platform2)));
    NUI_CHECK(ui::handled(destroyed_tree.dispatch(key_up(ui::Key::Down), platform2)));
    NUI_CHECK(ui::handled(destroyed_tree.dispatch(test::key(ui::Key::Down), platform2)));
    present.set(false);
    (void)destroyed_tree.dispatch(test::key(ui::Key::Enter), platform2);
    NUI_CHECK(!present.get());
    NUI_CHECK(selection.get() == 1);
}

void visibility_anchor_close_contract(ui::VisibilityMode unavailable_mode) {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    auto after = std::make_shared<test::ProbeState>();
    after->input_result = ui::EventResult::Handled;

    ui::UI tree{ui::Column{
        ui::Visibility{
            visibility,
            ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}}},
        test::Probe{after},
    }};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));

    visibility.set(unavailable_mode);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(after->focus_in >= 1);
    NUI_CHECK(after->key_events == 1);
}

void hidden_and_collapsed_anchor_close_contract() {
    visibility_anchor_close_contract(ui::VisibilityMode::Hidden);
    visibility_anchor_close_contract(ui::VisibilityMode::Collapsed);
}

void popup_menu_empty_callback_is_not_actionable_contract() {
    test::MockPlatform platform;
    int actions = 0;

    ui::UI tree{ui::PopupMenu{
        "Actions",
        {
            ui::PopupMenuItem::action("No callback", {}),
            ui::PopupMenuItem::action("Run", [&] { ++actions; }),
        }}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(actions == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
}

void popup_menu_contract() {
    test::MockPlatform platform;
    ui::State<bool> read_only{true};
    int disabled_actions = 0;
    int enabled_actions = 0;

    ui::UI tree{ui::ReadOnly{
        read_only,
        ui::PopupMenu{
            "Actions",
            {
                ui::PopupMenuItem::action("Disabled", [&] { ++disabled_actions; }, false),
                ui::PopupMenuItem::separator(),
                ui::PopupMenuItem::action("Run", [&] { ++enabled_actions; }),
            }}}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
    NUI_CHECK(disabled_actions == 0);
    NUI_CHECK(enabled_actions == 0);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(disabled_actions == 0);
    NUI_CHECK(enabled_actions == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
}

void popup_menu_immutable_snapshot_contract() {
    test::MockPlatform platform;
    int old_actions = 0;
    int new_actions = 0;
    int provider_calls = 0;
    std::vector<ui::PopupMenuItem> model{
        ui::PopupMenuItem::action("Old", [&] { ++old_actions; })};

    ui::UI tree{ui::PopupMenu{"Actions", [&] {
        ++provider_calls;
        return model;
    }}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(provider_calls == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    model = {ui::PopupMenuItem::action("New", [&] { ++new_actions; })};
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(old_actions == 1);
    NUI_CHECK(new_actions == 0);
    NUI_CHECK(provider_calls == 1);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(provider_calls == 2);
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(old_actions == 1);
    NUI_CHECK(new_actions == 1);
}

void popup_menu_pointer_non_action_contract() {
    test::MockPlatform platform;
    int disabled_actions = 0;
    int enabled_actions = 0;

    ui::Theme theme = ui::default_theme();
    theme.controls.control_height = 40.0f;
    theme.controls.minimum_width = 120.0f;
    theme.spacing.sm = 10.0f;

    ui::UI tree{
        ui::Column{
            ui::PopupMenu{
                "Actions",
                {
                    ui::PopupMenuItem::action("Disabled", [&] { ++disabled_actions; }, false),
                    ui::PopupMenuItem::separator(),
                    ui::PopupMenuItem::action("Run", [&] { ++enabled_actions; }),
                }},
            ui::Spacer{1.0f, 120.0f}},
        theme};
    tree.resize({240.0f, 220.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));

    // Column padding places the 40 px anchor at y=24, so the popup starts at
    // y=64: disabled action [64,104), separator [104,114), enabled [114,154).
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 80.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 80.0f), platform)));
    NUI_CHECK(disabled_actions == 0);
    NUI_CHECK(enabled_actions == 0);

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 108.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 108.0f), platform)));
    NUI_CHECK(disabled_actions == 0);
    NUI_CHECK(enabled_actions == 0);

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 130.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 130.0f), platform)));
    NUI_CHECK(disabled_actions == 0);
    NUI_CHECK(enabled_actions == 1);
}

void pointer_commit_and_no_click_through_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    int underlying_activations = 0;

    std::vector<ui::Spec> children;
    children.push_back(ui::make_spec(ui::ComboBox<int>{
        selection,
        {{1, "One", true}, {2, "Disabled", false}, {3, "Three", true}}}));
    children.push_back(ui::make_spec(
        ui::Button{"Underlying", [&] { ++underlying_activations; }}));

    ui::UI tree{FixedRoot{std::move(children)}};
    tree.resize({240.0f, 220.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 30.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 30.0f), platform)));

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 120.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 120.0f), platform)));
    NUI_CHECK(selection.get() == 1);

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 160.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 160.0f), platform)));
    NUI_CHECK(selection.get() == 3);
    NUI_CHECK(underlying_activations == 0);

    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 30.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerUp, 30.0f, 30.0f), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 30.0f, 190.0f), platform)));
    (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 30.0f, 190.0f), platform);
    NUI_CHECK(underlying_activations == 0);
}

void reentrant_menu_callback_contract() {
    test::MockPlatform platform;
    ui::State<bool> present{true};
    ui::UI* tree_ptr = nullptr;
    ui::OverlayHandle replacement;
    int actions = 0;

    auto menu = ui::PopupMenu{
        "Actions",
        {ui::PopupMenuItem::action("Replace", [&] {
            ++actions;
            present.set(false);
            ui::OverlaySpec overlay;
            overlay.mode = ui::OverlayMode::Modal;
            overlay.content = ui::make_spec(ui::Button{"Replacement", [] {}});
            replacement = tree_ptr->show_overlay(std::move(overlay));
        })}};
    ui::UI tree{ui::If{present, std::move(menu)}};
    tree_ptr = &tree;
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Enter), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(actions == 1);
    NUI_CHECK(!present.get());
    NUI_CHECK(replacement.valid());
}

void dynamic_theme_inheritance_contract() {
    test::MockPlatform platform;
    ui::State<bool> present{false};
    auto probe = std::make_shared<ThemeProbeState>();

    ui::Theme theme = ui::default_theme();
    theme.controls.control_height = 73.0f;

    ui::UI tree{ui::If{present, ThemeProbe{probe}}, theme};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);
    NUI_CHECK(probe->mounted_control_height < 0.0f);

    present.set(true);
    tree.resize({240.0f, 180.0f});
    NUI_CHECK(probe->mounted_control_height == 73.0f);
}

void headless_open_highlight_contract() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::UI tree{ui::ComboBox<int>{
        selection,
        {{1, "One", true}, {2, "Disabled", false}, {3, "Three", true}}}};
    tree.resize({240.0f, 180.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{240.0f, 180.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const auto closed = renderer.rgba_pixels();

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(renderer.render(tree));
    const auto open_selected = renderer.rgba_pixels();
    NUI_CHECK(open_selected != closed);

    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(selection.get() == 1);
    NUI_CHECK(renderer.render(tree));
    const auto open_next = renderer.rgba_pixels();
    NUI_CHECK(open_next != open_selected);
}

void independent_views_contract() {
    test::MockPlatform platform_a;
    test::MockPlatform platform_b;
    ui::State<int> a_selection{1};
    ui::State<int> b_selection{10};

    ui::UI a{ui::ComboBox<int>{a_selection, {{1, "A1", true}, {2, "A2", true}}}};
    ui::UI b{ui::ComboBox<int>{b_selection, {{10, "B1", true}, {20, "B2", true}}}};
    a.resize({220.0f, 160.0f});
    b.resize({220.0f, 160.0f});
    a.activate(platform_a);
    b.activate(platform_b);

    NUI_CHECK(ui::handled(a.dispatch(test::key(ui::Key::Down), platform_a)));
    NUI_CHECK(ui::handled(b.dispatch(test::key(ui::Key::Down), platform_b)));
    NUI_CHECK(ui::handled(a.dispatch(key_up(ui::Key::Down), platform_a)));
    NUI_CHECK(ui::handled(b.dispatch(key_up(ui::Key::Down), platform_b)));
    NUI_CHECK(ui::handled(a.dispatch(test::key(ui::Key::Down), platform_a)));
    NUI_CHECK(ui::handled(a.dispatch(test::key(ui::Key::Enter), platform_a)));
    NUI_CHECK(a_selection.get() == 2);
    NUI_CHECK(b_selection.get() == 10);
}

void cancelled_popup_reopens_without_stale_anchor_or_opener_latch() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::UI tree{ui::Column{
        ui::ComboBox<int>{selection, {{1, "One", true}, {2, "Two", true}}},
        ui::Button{"After", [] {}}}};
    tree.resize({240, 180});
    tree.activate(platform);
    for (int pass = 0; pass != 3; ++pass) {
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
        NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
        NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Escape), platform)));
        NUI_CHECK(selection.get() == 1);
    }
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Tab), platform)));
    tree.dispatch(key_up(ui::Key::Down), platform);
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Tab, true), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(key_up(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Down), platform)));
    NUI_CHECK(ui::handled(tree.dispatch(test::key(ui::Key::Enter), platform)));
    NUI_CHECK(selection.get() == 2);
}

void nested_provider_open_preserves_the_newer_popup() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    ui::UI* owner = nullptr;
    int calls = 0;
    ui::UI tree{ui::ComboBox<int>{selection, [&] {
        const int call = ++calls;
        if (call == 2) {
            owner->dispatch(test::key(ui::Key::Down),platform);
            return std::vector<ui::ComboBoxOption<int>>{{9,"Outer stale",true}};
        }
        if (call >= 3)
            return std::vector<ui::ComboBoxOption<int>>{{7,"Nested accepted",true}};
        return std::vector<ui::ComboBoxOption<int>>{{1,"One",true}};
    }}};
    owner = &tree;
    tree.resize({240,180});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240,180},1};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(calls == 1);
    tree.dispatch(test::key(ui::Key::Down),platform);
    NUI_CHECK(calls == 3 && tree.overlay_entries().size() == 1);
    tree.dispatch(key_up(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(selection.get() == 7 && tree.overlay_entries().empty());
    tree.deactivate(platform);
}

struct NestedOpenBoundary {
    bool armed{};
    std::function<void()> invoke;
};
struct CopyOpeningProvider {
    std::shared_ptr<NestedOpenBoundary> boundary;
    std::shared_ptr<int> calls;
    CopyOpeningProvider(std::shared_ptr<NestedOpenBoundary> value,std::shared_ptr<int> count)
        : boundary(std::move(value)),calls(std::move(count)) {}
    CopyOpeningProvider(const CopyOpeningProvider& other)
        : boundary(other.boundary),calls(other.calls) {
        if (std::exchange(boundary->armed,false)) boundary->invoke();
    }
    std::vector<ui::ComboBoxOption<int>> operator()() const {
        const int call = ++*calls;
        if (call == 1) return {{1,"One",true}};
        return call == 2 ? std::vector<ui::ComboBoxOption<int>>{{7,"Nested accepted",true}}
                         : std::vector<ui::ComboBoxOption<int>>{{9,"Outer stale",true}};
    }
};
void provider_copy_does_not_run_the_superseded_provider() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    auto boundary = std::make_shared<NestedOpenBoundary>();
    auto calls = std::make_shared<int>();
    ui::UI tree{ui::ComboBox<int>{selection,CopyOpeningProvider{boundary,calls}}};
    tree.resize({240,180});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240,180},1};
    NUI_CHECK(renderer.render(tree));
    boundary->invoke = [&] { tree.dispatch(test::key(ui::Key::Down),platform); };
    boundary->armed = true;
    tree.dispatch(test::key(ui::Key::Down),platform);
    NUI_CHECK(!boundary->armed && *calls == 2 && tree.overlay_entries().size() == 1);
    boundary->invoke = {};
    tree.dispatch(key_up(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(selection.get() == 7 && tree.overlay_entries().empty());
}

struct EqualityOpeningChoice {
    int id{};
    std::shared_ptr<NestedOpenBoundary> boundary;
    bool operator==(const EqualityOpeningChoice& other) const {
        if (boundary && std::exchange(boundary->armed,false)) boundary->invoke();
        return id == other.id;
    }
};
void selection_equality_preserves_a_newer_open_attempt() {
    test::MockPlatform platform;
    auto boundary = std::make_shared<NestedOpenBoundary>();
    ui::State<EqualityOpeningChoice> selection{{1,boundary}};
    int calls = 0;
    ui::UI tree{ui::ComboBox<EqualityOpeningChoice>{selection,[&] {
        const int call = ++calls;
        // Arm after retained checkpoint equality has completed, at the outer
        // opener's provider boundary immediately before selection equality.
        if (call == 2) boundary->armed = true;
        const int id = call >= 3 ? 7 : 1;
        return std::vector<ui::ComboBoxOption<EqualityOpeningChoice>>{
            {{id,boundary},id == 7 ? "Nested accepted" : "Outer stale",true}};
    }}};
    tree.resize({240,180});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240,180},1};
    NUI_CHECK(renderer.render(tree));
    boundary->invoke = [&] { tree.dispatch(test::key(ui::Key::Down),platform); };
    tree.dispatch(test::key(ui::Key::Down),platform);
    NUI_CHECK(!boundary->armed && calls == 3 && tree.overlay_entries().size() == 1);
    boundary->invoke = {};
    tree.dispatch(key_up(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(selection.get().id == 7 && tree.overlay_entries().empty());
}

void opening_invalidation_preserves_the_newer_popup() {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    int calls = 0;
    ui::UI tree{ui::ComboBox<int>{selection,[&] {
        const int call = ++calls;
        if (call == 1) return std::vector<ui::ComboBoxOption<int>>{{1,"One",true}};
        if (call == 2) return std::vector<ui::ComboBoxOption<int>>{{1,"Outer stale",true}};
        return std::vector<ui::ComboBoxOption<int>>{{7,"Nested accepted",true}};
    }}};
    tree.resize({240,180});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240,180},1};
    NUI_CHECK(renderer.render(tree));
    bool armed = true;
    tree.set_invalidation_callback([&] {
        if (std::exchange(armed,false))
            tree.dispatch(test::key(ui::Key::Down),platform);
    });
    tree.dispatch(test::key(ui::Key::Down),platform);
    tree.clear_invalidation_callback();
    NUI_CHECK(!armed && calls == 3 && tree.overlay_entries().size() == 1);
    tree.dispatch(key_up(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(selection.get() == 7 && tree.overlay_entries().empty());
}

void failed_pressed_invalidation_preserves_only_the_current_gesture(
    bool panel, bool on_up, bool nested) {
    test::MockPlatform platform;
    ui::State<int> selection{1};
    int writes = 0;
    auto observer = selection.observe([&](const auto&) { ++writes; });
    ui::ComboBoxStyle anchor_style;
    anchor_style.pressed.fill = ui::Color{1,0,0,1};
    ui::MenuItemStyle row_style;
    row_style.pressed.text = ui::Color{1,0,0,1};
    std::vector<ui::Spec> children;
    children.push_back(ui::make_spec(ui::ComboBox<int>{
        selection, {{1,"One",true},{2,"Two",true}}}
        .style(anchor_style).item_style(row_style)));
    ui::UI tree{FixedRoot{std::move(children)}};
    tree.resize({240,220});
    tree.activate(platform);
    if (panel) {
        tree.dispatch(test::key(ui::Key::Down),platform);
        tree.dispatch(key_up(ui::Key::Down),platform);
        NUI_CHECK(tree.overlay_entries().size() == 1);
    }
    const float y = panel ? 120.0f : 30.0f;
    // Establish hover/highlight before arming the fault, so it is the pressed
    // transition rather than the preceding pointer-hover callback that fails.
    tree.dispatch(test::pointer(ui::InputType::PointerMove,30,y),platform);
    ui::HeadlessRenderer renderer{{240,220},1};
    NUI_CHECK(renderer.render(tree));
    if (on_up) {
        tree.dispatch(test::pointer(ui::InputType::PointerDown,30,y),platform);
        NUI_CHECK(renderer.render(tree));
    }
    bool armed = true;
    int fault_calls = 0;
    tree.set_invalidation_callback([&] {
        if (!std::exchange(armed,false)) return;
        ++fault_calls;
        if (nested)
            tree.dispatch(test::pointer(ui::InputType::PointerDown,30,y),platform);
        throw std::runtime_error("combo pressed invalidation");
    });
    bool caught = false;
    try {
        tree.dispatch(test::pointer(on_up ? ui::InputType::PointerUp
                                        : ui::InputType::PointerDown,30,y),platform);
    } catch (const std::runtime_error& error) {
        caught = std::string_view{error.what()} == "combo pressed invalidation";
    }
    tree.clear_invalidation_callback();
    NUI_CHECK(caught && !armed && fault_calls == 1 && selection.get() == 1 && writes == 0);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,30,y),platform);
    if (!nested) {
        NUI_CHECK(selection.get() == 1 && writes == 0);
        NUI_CHECK(tree.overlay_entries().size() == (panel ? 1U : 0U));
        tree.dispatch(test::pointer(ui::InputType::PointerDown,30,y),platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp,30,y),platform);
    }
    NUI_CHECK(selection.get() == (panel ? 2 : 1) && writes == (panel ? 1 : 0));
    NUI_CHECK(tree.overlay_entries().size() == (panel ? 0U : 1U));
}

void failed_panel_down_does_not_commit() {
    failed_pressed_invalidation_preserves_only_the_current_gesture(true,false,false);
}
void failed_panel_up_does_not_commit() {
    failed_pressed_invalidation_preserves_only_the_current_gesture(true,true,false);
}
void failed_anchor_down_does_not_open() {
    failed_pressed_invalidation_preserves_only_the_current_gesture(false,false,false);
}
void failed_anchor_up_does_not_open() {
    failed_pressed_invalidation_preserves_only_the_current_gesture(false,true,false);
}
void nested_press_survives_an_older_invalidation_failure() {
    for (const bool panel : {false,true})
        for (const bool on_up : {false,true})
            failed_pressed_invalidation_preserves_only_the_current_gesture(panel,on_up,true);
}

void terminal_anchor_callback_failure_does_not_restore_old_latches() {
    for (const bool deactivate : {false,true}) {
        for (const bool nested : {false,true}) {
            test::MockPlatform platform;
            ui::State<int> selection{1};
            ui::ComboBoxStyle style;
            style.pressed.fill = ui::Color{1,0,0,1};
            auto node = ui::compile(ui::make_spec(ui::ComboBox<int>{
                selection, {{1,"One",true},{2,"Two",true}}}.style(style)));
            auto& component = *node->component;
            const auto noop = [] {};
            ui::MountContext mount{1,noop,noop,noop};
            component.mount(mount);
            ui::InputContext input{{20,20,120,40},platform,noop,noop,noop,noop};
            ui::FocusContext focus{{20,20,120,40},platform,noop,noop};
            component.focus_changed(true,focus);
            component.input(test::key(ui::Key::Space),input);
            bool armed = true;
            const auto fail = [&] {
                if (!std::exchange(armed,false)) return;
                if (nested) {
                    component.focus_changed(true,focus);
                    component.input(test::key(ui::Key::Space),input);
                }
                throw std::runtime_error("terminal combo callback");
            };
            ui::FocusContext failing_focus{{20,20,120,40},platform,fail,noop};
            ui::LifecycleContext failing_lifecycle{1,{20,20,120,40},fail,noop};
            bool caught = false;
            try {
                if (deactivate) component.deactivate(failing_lifecycle);
                else component.focus_changed(false,failing_focus);
            } catch (const std::runtime_error& error) {
                caught = std::string_view{error.what()} == "terminal combo callback";
            }
            NUI_CHECK(caught && !armed);
            component.focus_changed(true,focus);
            component.input(key_up(ui::Key::Space),input);
            auto* commands = dynamic_cast<ui::detail::OverlayCommandSource*>(&component);
            NUI_CHECK(commands);
            NUI_CHECK(bool(commands->take_overlay_command()) == nested);
            NUI_CHECK(!commands->take_overlay_command());
            if (!nested) {
                component.input(test::key(ui::Key::Space),input);
                component.input(key_up(ui::Key::Space),input);
                NUI_CHECK(commands->take_overlay_command());
                NUI_CHECK(!commands->take_overlay_command());
            }
            ui::LifecycleContext lifecycle{1,{20,20,120,40},noop,noop};
            component.unmount(lifecycle);
        }
    }
}

void suite() {
    nested_provider_open_preserves_the_newer_popup();
    provider_copy_does_not_run_the_superseded_provider();
    selection_equality_preserves_a_newer_open_attempt();
    opening_invalidation_preserves_the_newer_popup();
    failed_panel_down_does_not_commit();
    failed_panel_up_does_not_commit();
    failed_anchor_down_does_not_open();
    failed_anchor_up_does_not_open();
    nested_press_survives_an_older_invalidation_failure();
    terminal_anchor_callback_failure_does_not_restore_old_latches();
    cancelled_popup_reopens_without_stale_anchor_or_opener_latch();
    combo_keyboard_commit_contract();
    no_match_home_end_contract();
    immutable_open_snapshot_contract();
    provider_availability_reentrancy_contract();
    tab_escape_and_read_only_contract();
    disabled_and_destroyed_anchor_close_contract();
    hidden_and_collapsed_anchor_close_contract();
    popup_menu_empty_callback_is_not_actionable_contract();
    popup_menu_contract();
    popup_menu_immutable_snapshot_contract();
    popup_menu_pointer_non_action_contract();
    pointer_commit_and_no_click_through_contract();
    reentrant_menu_callback_contract();
    dynamic_theme_inheritance_contract();
    headless_open_highlight_contract();
    independent_views_contract();
}

} // namespace

int main(int argc,char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "all";
    if (mode == "nested_provider_open") return test::run(mode.data(),&nested_provider_open_preserves_the_newer_popup);
    if (mode == "provider_copy_open") return test::run(mode.data(),&provider_copy_does_not_run_the_superseded_provider);
    if (mode == "equality_open") return test::run(mode.data(),&selection_equality_preserves_a_newer_open_attempt);
    if (mode == "invalidation_open") return test::run(mode.data(),&opening_invalidation_preserves_the_newer_popup);
    if (mode == "failed_panel_down") return test::run(mode.data(),&failed_panel_down_does_not_commit);
    if (mode == "failed_panel_up") return test::run(mode.data(),&failed_panel_up_does_not_commit);
    if (mode == "failed_anchor_down") return test::run(mode.data(),&failed_anchor_down_does_not_open);
    if (mode == "failed_anchor_up") return test::run(mode.data(),&failed_anchor_up_does_not_open);
    if (mode == "nested_press") return test::run(mode.data(),&nested_press_survives_an_older_invalidation_failure);
    if (mode == "terminal_anchor") return test::run(mode.data(),&terminal_anchor_callback_failure_does_not_restore_old_latches);
    return test::run("t035_combo_popup", &suite);
}
