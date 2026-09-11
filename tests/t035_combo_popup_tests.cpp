#include "test_support.hpp"

#include <nativeui/detail/theme_binding.hpp>

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

void suite() {
    combo_keyboard_commit_contract();
    no_match_home_end_contract();
    immutable_open_snapshot_contract();
    tab_escape_and_read_only_contract();
    disabled_and_destroyed_anchor_close_contract();
    popup_menu_empty_callback_is_not_actionable_contract();
    popup_menu_contract();
    popup_menu_pointer_non_action_contract();
    pointer_commit_and_no_click_through_contract();
    reentrant_menu_callback_contract();
    dynamic_theme_inheritance_contract();
    headless_open_highlight_contract();
    independent_views_contract();
}

} // namespace

int main() { return test::run("t035_combo_popup", &suite); }
