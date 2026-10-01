#include "test_support.hpp"

#include <nativeui/detail/semantic_tree.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>
#include <nativeui/nativeui.hpp>
#include <nativeui/semantics.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define ACCESSIBILITY_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

[[nodiscard]] bool near(float actual, float expected) {
    return std::fabs(actual - expected) <= 1e-4f;
}

[[nodiscard]] ui::detail::SemanticActionRequest action_request(
    ui::SemanticAction action) {
    ui::detail::SemanticActionRequest request;
    request.action = action;
    return request;
}

[[nodiscard]] const ui::SemanticNodeSnapshot* find_role(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticRole role) {
    for (const auto& node : snapshot.nodes) {
        if (node.info.role == role) return &node;
    }
    return nullptr;
}

[[nodiscard]] const ui::SemanticNodeSnapshot* find_named_role(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticRole role,
    std::string_view name) {
    for (const auto& node : snapshot.nodes) {
        if (node.info.role == role && node.info.name == name) return &node;
    }
    return nullptr;
}

[[nodiscard]] const ui::SemanticNodeSnapshot* find_node(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticId id) {
    for (const auto& node : snapshot.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

[[nodiscard]] std::vector<ui::SemanticRole> child_roles(
    const ui::SemanticTreeSnapshot& snapshot,
    const ui::SemanticNodeSnapshot& node) {
    std::vector<ui::SemanticRole> roles;
    roles.reserve(node.children.size());
    for (const auto id : node.children) {
        if (const auto* child = find_node(snapshot, id)) {
            roles.push_back(child->info.role);
        }
    }
    return roles;
}

[[nodiscard]] std::vector<const ui::SemanticNodeSnapshot*> nodes_with_role(
    const ui::SemanticTreeSnapshot& snapshot,
    ui::SemanticRole role) {
    std::vector<const ui::SemanticNodeSnapshot*> result;
    for (const auto& node : snapshot.nodes) {
        if (node.info.role == role) result.push_back(&node);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Component-level projections through the production tree/action path
// ---------------------------------------------------------------------------

void text_input_component_projection_contract() {
    ui::State<std::string> value{"Ada"};
    auto root = ui::compile(ui::make_spec(
        ui::TextInput{"Display name", value}.placeholder("name").max_length(12)));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    const auto info = root->component->semantics();
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::TextInput);
    ACCESSIBILITY_CHECK(info.name == "Display name");
    ACCESSIBILITY_CHECK(info.text_value.has_value());
    ACCESSIBILITY_CHECK(*info.text_value == "Ada");
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(!info.read_only);
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Focus));
    ACCESSIBILITY_CHECK(!info.supports(ui::SemanticAction::Toggle));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::TextInput);
    ACCESSIBILITY_CHECK(node != nullptr);
    ACCESSIBILITY_CHECK(node->info.text_value.has_value());
    ACCESSIBILITY_CHECK(*node->info.text_value == "Ada");

    const ui::detail::SemanticIdentity identity{node_id, std::nullopt};
    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"Grace\nHopper"};
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, set_value));
    ACCESSIBILITY_CHECK(value.get() == "Grace Hopper");

    // Semantic writes keep the widget's max-length policy.
    set_value.text_value = std::string{"0123456789abcdef"};
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, set_value));
    ACCESSIBILITY_CHECK(value.get() == "0123456789ab");

    const auto live = ui::detail::SemanticTreeActionAccess::current_semantics(
        tree, identity);
    ACCESSIBILITY_CHECK(live.has_value());
    ACCESSIBILITY_CHECK(live->text_value.has_value());
    ACCESSIBILITY_CHECK(*live->text_value == "0123456789ab");

    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Increment)));
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Toggle)));
}

void text_area_component_projection_contract() {
    ui::State<std::string> value{"line one"};
    auto root = ui::compile(ui::make_spec(ui::TextArea{"Notes", value}));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    const auto info = root->component->semantics();
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::TextArea);
    ACCESSIBILITY_CHECK(info.name == "Notes");
    ACCESSIBILITY_CHECK(info.text_value.has_value());
    ACCESSIBILITY_CHECK(*info.text_value == "line one");
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::SetValue));
    ACCESSIBILITY_CHECK(info.supports(ui::SemanticAction::Focus));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::TextArea);
    ACCESSIBILITY_CHECK(node != nullptr);
    ACCESSIBILITY_CHECK(node->info.name == "Notes");

    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"line one\nline two"};
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{node_id, std::nullopt}, set_value));
    ACCESSIBILITY_CHECK(value.get() == "line one\nline two");
}

void semantic_value_writes_revalidate_read_only() {
    ui::State<bool> read_only{true};
    ui::State<std::string> value{"Ada"};
    auto root = ui::compile(ui::make_spec(
        ui::ReadOnly{read_only, ui::TextInput{"Locked", value}}));
    ui::Node* root_ptr = root.get();
    const auto text_id = static_cast<ui::SemanticId>(root_ptr->children.front()->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::TextInput);
    ACCESSIBILITY_CHECK(node != nullptr);
    ACCESSIBILITY_CHECK(node->info.read_only);
    ACCESSIBILITY_CHECK(node->info.enabled);
    ACCESSIBILITY_CHECK(node->info.focusable);
    ACCESSIBILITY_CHECK(node->info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"Grace"};
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{text_id, std::nullopt}, set_value));
    ACCESSIBILITY_CHECK(value.get() == "Ada");
}

void disabled_widget_projection_removes_actions() {
    ui::State<bool> enabled{false};
    ui::State<float> gain{0.25f};
    auto root = ui::compile(ui::make_spec(
        ui::Enabled{enabled, ui::Slider{gain}.range(0.0f, 1.0f).step(0.05f)}));
    ui::Node* root_ptr = root.get();
    const auto slider_id = static_cast<ui::SemanticId>(root_ptr->children.front()->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::Slider);
    ACCESSIBILITY_CHECK(node != nullptr);
    ACCESSIBILITY_CHECK(!node->info.enabled);
    ACCESSIBILITY_CHECK(!node->info.focusable);
    ACCESSIBILITY_CHECK(node->info.actions.empty());

    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{slider_id, std::nullopt},
        action_request(ui::SemanticAction::Increment)));
    ACCESSIBILITY_CHECK(near(gain.get(), 0.25f));
}

void tabs_component_projection_contract() {
    ui::State<int> selected{2};
    auto root = ui::compile(ui::make_spec(ui::Tabs<int>{selected}
        .tab(1, "One", ui::Spacer{100.0f, 40.0f})
        .tab(2, "Two", ui::Spacer{100.0f, 40.0f})
        .tab(3, "Three", ui::Spacer{100.0f, 40.0f}, false)));
    ui::Node* root_ptr = root.get();

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    ACCESSIBILITY_CHECK(snapshot.root == static_cast<ui::SemanticId>(root_ptr->id));
    const auto& tabs = snapshot.nodes.front();
    ACCESSIBILITY_CHECK(tabs.info.role == ui::SemanticRole::Tabs);
    ACCESSIBILITY_CHECK(tabs.info.focusable);
    ACCESSIBILITY_CHECK(tabs.info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    // The selected TabPanel is visible; collapsed panels are absent while the
    // three Tab nodes stay in logical order.
    const auto roles = child_roles(snapshot, tabs);
    ACCESSIBILITY_CHECK(roles == std::vector<ui::SemanticRole>({
        ui::SemanticRole::Tab,
        ui::SemanticRole::Tab,
        ui::SemanticRole::TabPanel,
        ui::SemanticRole::Tab,
    }));

    const auto* one = find_named_role(snapshot, ui::SemanticRole::Tab, "One");
    const auto* two = find_named_role(snapshot, ui::SemanticRole::Tab, "Two");
    const auto* three = find_named_role(snapshot, ui::SemanticRole::Tab, "Three");
    ACCESSIBILITY_CHECK(one != nullptr);
    ACCESSIBILITY_CHECK(two != nullptr);
    ACCESSIBILITY_CHECK(three != nullptr);
    ACCESSIBILITY_CHECK(!one->info.selected);
    ACCESSIBILITY_CHECK(two->info.selected);
    ACCESSIBILITY_CHECK(two->info.enabled);
    ACCESSIBILITY_CHECK(two->info.focusable);
    ACCESSIBILITY_CHECK(two->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Select, ui::SemanticAction::Focus}));
    ACCESSIBILITY_CHECK(!three->info.enabled);
    ACCESSIBILITY_CHECK(three->info.actions.empty());

    const auto* panel = find_role(snapshot, ui::SemanticRole::TabPanel);
    ACCESSIBILITY_CHECK(panel != nullptr);
    ACCESSIBILITY_CHECK(panel->info.enabled);
    ACCESSIBILITY_CHECK(!panel->info.focusable);
    ACCESSIBILITY_CHECK(panel->info.actions.empty());

    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{one->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    ACCESSIBILITY_CHECK(selected.get() == 1);

    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{three->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    ACCESSIBILITY_CHECK(selected.get() == 1);
}

void tabs_focus_action_targets_the_composite_owner() {
    test::MockPlatform platform;
    ui::State<int> selected{1};
    auto root = ui::compile(ui::make_spec(ui::Column{
        ui::Button{"Outside", [] {}},
        ui::Tabs<int>{selected}
            .tab(1, "One", ui::Spacer{100.0f, 40.0f})
            .tab(2, "Two", ui::Spacer{100.0f, 40.0f})}));

    ui::Tree tree{std::move(root)};
    tree.mount();
    tree.layout({200.0f, 120.0f});
    tree.activate_focus(platform);

    const auto initial = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    ACCESSIBILITY_CHECK(initial.has_value());
    const auto* tabs_initial = find_role(*initial, ui::SemanticRole::Tabs);
    const auto* button_initial = find_role(*initial, ui::SemanticRole::Button);
    ACCESSIBILITY_CHECK(tabs_initial != nullptr);
    ACCESSIBILITY_CHECK(button_initial != nullptr);
    ACCESSIBILITY_CHECK(button_initial->info.focused);
    ACCESSIBILITY_CHECK(!tabs_initial->info.focused);

    const auto* tab = find_named_role(*initial, ui::SemanticRole::Tab, "Two");
    ACCESSIBILITY_CHECK(tab != nullptr);
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{tab->id, std::nullopt},
        action_request(ui::SemanticAction::Focus)));

    const auto focused = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    ACCESSIBILITY_CHECK(focused.has_value());
    const auto* tabs_focused = find_role(*focused, ui::SemanticRole::Tabs);
    const auto* button_focused = find_role(*focused, ui::SemanticRole::Button);
    ACCESSIBILITY_CHECK(tabs_focused != nullptr);
    ACCESSIBILITY_CHECK(button_focused != nullptr);
    ACCESSIBILITY_CHECK(tabs_focused->info.focused);
    ACCESSIBILITY_CHECK(!button_focused->info.focused);
}

void list_view_component_projection_contract() {
    ui::State<std::optional<std::string>> selected{std::nullopt};
    int activations = 0;
    std::string activated_key;
    auto root = ui::compile(ui::make_spec(ui::ListView<std::string>{selected}
        .item("one", ui::Label{"One"})
        .item("two", ui::Label{"Two"})
        .item("three", ui::Label{"Three"}, false)
        .on_activate([&](const std::string& key) {
            ++activations;
            activated_key = key;
        })));
    ui::Node* root_ptr = root.get();

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    ACCESSIBILITY_CHECK(snapshot.root == static_cast<ui::SemanticId>(root_ptr->id));
    const auto* list = find_role(snapshot, ui::SemanticRole::ListView);
    ACCESSIBILITY_CHECK(list != nullptr);
    ACCESSIBILITY_CHECK(list->info.focusable);
    ACCESSIBILITY_CHECK(list->info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    ACCESSIBILITY_CHECK(child_roles(snapshot, *list) == std::vector<ui::SemanticRole>({
        ui::SemanticRole::ListItem,
        ui::SemanticRole::ListItem,
        ui::SemanticRole::ListItem,
    }));

    const auto* one = find_named_role(snapshot, ui::SemanticRole::ListItem, "one");
    const auto* two = find_named_role(snapshot, ui::SemanticRole::ListItem, "two");
    const auto* three = find_named_role(snapshot, ui::SemanticRole::ListItem, "three");
    ACCESSIBILITY_CHECK(one != nullptr);
    ACCESSIBILITY_CHECK(two != nullptr);
    ACCESSIBILITY_CHECK(three != nullptr);
    ACCESSIBILITY_CHECK(one->info.enabled);
    ACCESSIBILITY_CHECK(!one->info.selected);
    ACCESSIBILITY_CHECK(one->info.focusable);
    ACCESSIBILITY_CHECK(one->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Select,
        ui::SemanticAction::Focus,
        ui::SemanticAction::Activate,
    }));
    ACCESSIBILITY_CHECK(!three->info.enabled);
    ACCESSIBILITY_CHECK(three->info.actions.empty());

    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{two->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    ACCESSIBILITY_CHECK(selected.get().has_value());
    ACCESSIBILITY_CHECK(*selected.get() == "two");
    ACCESSIBILITY_CHECK(activations == 0);

    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{one->id, std::nullopt},
        action_request(ui::SemanticAction::Activate)));
    ACCESSIBILITY_CHECK(activations == 1);
    ACCESSIBILITY_CHECK(activated_key == "one");
    ACCESSIBILITY_CHECK(*selected.get() == "one");

    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{three->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    ACCESSIBILITY_CHECK(*selected.get() == "one");
}

void range_slider_component_projection_contract() {
    ui::State<ui::RangeValue> range{ui::RangeValue{0.2f, 0.8f}};
    auto root = ui::compile(ui::make_spec(
        ui::RangeSlider{range}.range(0.0f, 1.0f).step(0.1f)));
    ui::Node* root_ptr = root.get();

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    ACCESSIBILITY_CHECK(snapshot.nodes.size() == 2);
    const auto handles = nodes_with_role(snapshot, ui::SemanticRole::RangeSliderHandle);
    ACCESSIBILITY_CHECK(handles.size() == 2);
    const auto* lower = handles[0];
    const auto* upper = handles[1];
    ACCESSIBILITY_CHECK(lower->info.numeric_value.has_value());
    ACCESSIBILITY_CHECK(near(static_cast<float>(*lower->info.numeric_value), 0.2f));
    ACCESSIBILITY_CHECK(upper->info.numeric_value.has_value());
    ACCESSIBILITY_CHECK(near(static_cast<float>(*upper->info.numeric_value), 0.8f));
    for (const auto* handle : handles) {
        ACCESSIBILITY_CHECK(handle->info.focusable);
        ACCESSIBILITY_CHECK(handle->info.value_range.has_value());
        ACCESSIBILITY_CHECK(handle->info.value_range->minimum == 0.0);
        ACCESSIBILITY_CHECK(handle->info.value_range->maximum == 1.0);
        ACCESSIBILITY_CHECK(near(static_cast<float>(handle->info.value_range->step), 0.1f));
        ACCESSIBILITY_CHECK(handle->info.actions == std::vector<ui::SemanticAction>({
            ui::SemanticAction::Increment,
            ui::SemanticAction::Decrement,
            ui::SemanticAction::SetValue,
            ui::SemanticAction::Focus,
        }));
    }

    const ui::detail::SemanticIdentity lower_identity{lower->id, std::nullopt};
    const ui::detail::SemanticIdentity upper_identity{upper->id, std::nullopt};

    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Increment)));
    ACCESSIBILITY_CHECK(near(range.get().low, 0.3f));
    ACCESSIBILITY_CHECK(near(range.get().high, 0.8f));

    auto set_upper = action_request(ui::SemanticAction::SetValue);
    set_upper.numeric_value = 0.9;
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, set_upper));
    ACCESSIBILITY_CHECK(near(range.get().high, 0.9f));

    // The lower handle cannot cross the upper handle's current value.
    auto set_lower = action_request(ui::SemanticAction::SetValue);
    set_lower.numeric_value = 0.95;
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, set_lower));
    ACCESSIBILITY_CHECK(near(range.get().low, 0.9f));

    // The upper handle cannot cross the lower handle either.
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, action_request(ui::SemanticAction::Decrement)));
    ACCESSIBILITY_CHECK(near(range.get().high, 0.9f));

    // Once the lower handle moves down the upper handle can follow.
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Decrement)));
    ACCESSIBILITY_CHECK(near(range.get().low, 0.8f));
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, action_request(ui::SemanticAction::Decrement)));
    ACCESSIBILITY_CHECK(near(range.get().high, 0.8f));
}

void range_slider_read_only_and_focus_contract() {
    test::MockPlatform platform;
    ui::State<bool> read_only{false};
    ui::State<ui::RangeValue> range{ui::RangeValue{0.2f, 0.8f}};
    auto root = ui::compile(ui::make_spec(ui::Column{
        ui::Button{"Outside", [] {}},
        ui::ReadOnly{read_only, ui::RangeSlider{range}.range(0.0f, 1.0f).step(0.1f)}}));
    ui::Node* root_ptr = root.get();

    ui::Tree tree{std::move(root)};
    tree.mount();
    tree.layout({200.0f, 120.0f});
    tree.activate_focus(platform);

    read_only.set(true);
    tree.refresh_focus(platform);
    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto handles = nodes_with_role(snapshot, ui::SemanticRole::RangeSliderHandle);
    ACCESSIBILITY_CHECK(handles.size() == 2);
    for (const auto* handle : handles) {
        ACCESSIBILITY_CHECK(handle->info.read_only);
        ACCESSIBILITY_CHECK(handle->info.focusable);
        ACCESSIBILITY_CHECK(handle->info.actions ==
                   std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    }

    const ui::detail::SemanticIdentity lower_identity{handles[0]->id, std::nullopt};
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Increment)));
    ACCESSIBILITY_CHECK(near(range.get().low, 0.2f));

    read_only.set(false);
    tree.refresh_focus(platform);
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Focus)));
    const auto focused = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    ACCESSIBILITY_CHECK(focused.has_value());
    const auto* button = find_role(*focused, ui::SemanticRole::Button);
    ACCESSIBILITY_CHECK(button != nullptr);
    ACCESSIBILITY_CHECK(!button->info.focused);
}

void dialog_component_projection_contract() {
    ui::detail::DialogPanelLayout layout;
    layout.body_index = 0;
    ui::detail::DialogPanelComponent panel{layout, nullptr, nullptr, "Settings"};

    const auto info = panel.semantics();
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::Dialog);
    ACCESSIBILITY_CHECK(info.name == "Settings");
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    ACCESSIBILITY_CHECK(!info.supports(ui::SemanticAction::Activate));
}

void popup_menu_component_projection_contract() {
    std::vector<ui::PopupMenuItem> items;
    items.push_back(ui::PopupMenuItem::action("Open", [] {}));
    items.push_back(ui::PopupMenuItem::separator());
    items.push_back(ui::PopupMenuItem::action("Disabled", [] {}, false));

    auto root = ui::compile(ui::make_spec(ui::PopupMenu{"Actions", items}));
    const auto info = root->component->semantics();
    ACCESSIBILITY_CHECK(info.role == ui::SemanticRole::PopupMenu);
    ACCESSIBILITY_CHECK(info.name == "Actions");
    ACCESSIBILITY_CHECK(info.focusable);
    ACCESSIBILITY_CHECK(info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    auto session = std::make_shared<ui::detail::MenuPopupSession>();
    session->items = items;
    session->highlighted = 0;

    ui::detail::MenuPopupComponent popup{session};
    const auto popup_info = popup.semantics();
    ACCESSIBILITY_CHECK(popup_info.role == ui::SemanticRole::PopupMenu);
    ACCESSIBILITY_CHECK(popup_info.focusable);
    ACCESSIBILITY_CHECK(popup_info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    ui::detail::MenuPopupItemComponent open_item{session, 0};
    const auto open_info = open_item.semantics();
    ACCESSIBILITY_CHECK(open_info.role == ui::SemanticRole::MenuItem);
    ACCESSIBILITY_CHECK(open_info.name == "Open");
    ACCESSIBILITY_CHECK(open_info.enabled);
    ACCESSIBILITY_CHECK(open_info.selected);
    ACCESSIBILITY_CHECK(!open_info.focusable);
    ACCESSIBILITY_CHECK(open_info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Activate});

    ui::detail::MenuPopupItemComponent separator_item{session, 1};
    ACCESSIBILITY_CHECK(separator_item.semantics().role == ui::SemanticRole::None);

    ui::detail::MenuPopupItemComponent disabled_item{session, 2};
    const auto disabled_info = disabled_item.semantics();
    ACCESSIBILITY_CHECK(disabled_info.role == ui::SemanticRole::MenuItem);
    ACCESSIBILITY_CHECK(disabled_info.name == "Disabled");
    ACCESSIBILITY_CHECK(!disabled_info.enabled);
    ACCESSIBILITY_CHECK(!disabled_info.selected);
    ACCESSIBILITY_CHECK(disabled_info.actions.empty());

    std::vector<ui::ChildPlacement> placements(3);
    popup.layout_children({0.0f, 0.0f, 120.0f, 60.0f}, {}, placements);
    ACCESSIBILITY_CHECK(placements[0].bounds.h > 0.0f);
    ACCESSIBILITY_CHECK(placements[1].bounds.y >= placements[0].bounds.y + placements[0].bounds.h);
    ACCESSIBILITY_CHECK(placements[2].bounds.y >= placements[1].bounds.y + placements[1].bounds.h);
}

void combo_box_read_only_projection_is_focus_only() {
    ui::State<bool> read_only{true};
    ui::State<int> selection{99};
    const std::vector<ui::ComboBoxOption<int>> options{{1, "One", true}};
    auto root = ui::compile(ui::make_spec(
        ui::ReadOnly{read_only, ui::ComboBox<int>{selection, options}}));
    ui::Node* root_ptr = root.get();
    const auto combo_id = static_cast<ui::SemanticId>(root_ptr->children.front()->id);

    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::ComboBox);
    ACCESSIBILITY_CHECK(node != nullptr);
    ACCESSIBILITY_CHECK(node->info.read_only);
    ACCESSIBILITY_CHECK(node->info.enabled);
    ACCESSIBILITY_CHECK(node->info.focusable);
    ACCESSIBILITY_CHECK(node->info.expanded == ui::SemanticExpandedState::Collapsed);
    ACCESSIBILITY_CHECK(node->info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    ACCESSIBILITY_CHECK(!node->info.supports(ui::SemanticAction::Expand));
    ACCESSIBILITY_CHECK(!node->info.supports(ui::SemanticAction::Collapse));
    ACCESSIBILITY_CHECK(!node->info.supports(ui::SemanticAction::Select));

    // The live re-resolution used by the action router applies the same
    // ComboBox read-only projection, so no ExpandCollapse/Selection request can
    // reach the widget handler.
    const ui::detail::SemanticIdentity identity{combo_id, std::nullopt};
    const auto live = ui::detail::SemanticTreeActionAccess::current_semantics(
        tree, identity);
    ACCESSIBILITY_CHECK(live.has_value());
    ACCESSIBILITY_CHECK(live->actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    for (const auto action : {ui::SemanticAction::Expand,
                              ui::SemanticAction::Collapse,
                              ui::SemanticAction::Select}) {
        ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
            tree, identity, action_request(action)));
    }
    ACCESSIBILITY_CHECK(selection.get() == 99);
}

void list_view_read_only_activate_executes_once() {
    ui::State<bool> visible{true};
    ui::State<bool> read_only{true};
    ui::State<std::optional<std::string>> selection{std::nullopt};
    int activations = 0;
    std::string activated_key;
    auto root = ui::compile(ui::make_spec(ui::If{
        visible,
        ui::ReadOnly{
            read_only,
            ui::ListView<std::string>{selection}
                .item("one", ui::Label{"One"})
                .item("two", ui::Label{"Two"})
                .item("three", ui::Label{"Three"}, false)
                .on_activate([&](const std::string& key) {
                    ++activations;
                    activated_key = key;
                })}}));
    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* one = find_named_role(snapshot, ui::SemanticRole::ListItem, "one");
    const auto* three = find_named_role(snapshot, ui::SemanticRole::ListItem, "three");
    ACCESSIBILITY_CHECK(one != nullptr);
    ACCESSIBILITY_CHECK(three != nullptr);
    ACCESSIBILITY_CHECK(one->info.read_only);
    ACCESSIBILITY_CHECK(one->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Focus,
        ui::SemanticAction::Activate,
    }));
    ACCESSIBILITY_CHECK(!one->info.supports(ui::SemanticAction::Select));
    ACCESSIBILITY_CHECK(three->info.actions.empty());

    const ui::detail::SemanticIdentity one_identity{one->id, std::nullopt};
    const ui::detail::SemanticIdentity three_identity{three->id, std::nullopt};

    // Read-only Activate follows the read-only Space/`.on_activate` policy: the row
    // is selected and the callback runs exactly once per request.
    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, one_identity, action_request(ui::SemanticAction::Activate)));
    ACCESSIBILITY_CHECK(activations == 1);
    ACCESSIBILITY_CHECK(activated_key == "one");
    ACCESSIBILITY_CHECK(selection.get().has_value());
    ACCESSIBILITY_CHECK(*selection.get() == "one");

    ACCESSIBILITY_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, one_identity, action_request(ui::SemanticAction::Activate)));
    ACCESSIBILITY_CHECK(activations == 2);

    // The value-changing Select stays rejected for read-only rows.
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, one_identity, action_request(ui::SemanticAction::Select)));
    ACCESSIBILITY_CHECK(activations == 2);

    // Disabled rows reject Activate.
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, three_identity, action_request(ui::SemanticAction::Activate)));
    ACCESSIBILITY_CHECK(activations == 2);

    // Removing the retained subtree retires the row identity; a retained
    // native-proxy request fails closed.
    visible.set(false);
    tree.layout({200.0f, 120.0f});
    ACCESSIBILITY_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, one_identity, action_request(ui::SemanticAction::Activate)));
    ACCESSIBILITY_CHECK(activations == 2);
}

} // namespace

int main() {
    try {
        text_input_component_projection_contract();
        text_area_component_projection_contract();
        semantic_value_writes_revalidate_read_only();
        disabled_widget_projection_removes_actions();
        combo_box_read_only_projection_is_focus_only();
        list_view_read_only_activate_executes_once();
        tabs_component_projection_contract();
        tabs_focus_action_targets_the_composite_owner();
        list_view_component_projection_contract();
        range_slider_component_projection_contract();
        range_slider_read_only_and_focus_contract();
        dialog_component_projection_contract();
        popup_menu_component_projection_contract();
        std::cout << "PASS accessibility widget component semantics\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL accessibility widget component semantics: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
