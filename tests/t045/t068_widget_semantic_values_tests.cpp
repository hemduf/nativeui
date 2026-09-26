#include "test_support.hpp"

#include <nativeui/detail/semantic_tree.hpp>
#include <nativeui/detail/semantic_tree_action_access.hpp>
#include <nativeui/detail/semantic_widget_info.hpp>
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

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

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

void label_value_contract() {
    const auto info = ui::detail::label_semantic_info("Status ready");
    T068_CHECK(info.role == ui::SemanticRole::Text);
    T068_CHECK(info.name == "Status ready");
    T068_CHECK(!info.focusable);
    T068_CHECK(info.actions.empty());
}

void checkbox_value_contract() {
    const auto checked = ui::detail::checkbox_semantic_info("Enabled", true);
    T068_CHECK(checked.role == ui::SemanticRole::Checkbox);
    T068_CHECK(checked.name == "Enabled");
    T068_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(checked.focusable);
    T068_CHECK(checked.supports(ui::SemanticAction::Toggle));
    T068_CHECK(checked.supports(ui::SemanticAction::Focus));
    T068_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::checkbox_semantic_info("Enabled", false);
    T068_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void radio_value_contract() {
    const auto selected = ui::detail::radio_button_semantic_info("Mode A", true);
    T068_CHECK(selected.role == ui::SemanticRole::RadioButton);
    T068_CHECK(selected.name == "Mode A");
    T068_CHECK(selected.selected);
    T068_CHECK(selected.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(selected.focusable);
    T068_CHECK(selected.supports(ui::SemanticAction::Select));
    T068_CHECK(selected.supports(ui::SemanticAction::Focus));
    T068_CHECK(!selected.supports(ui::SemanticAction::Toggle));

    const auto unselected = ui::detail::radio_button_semantic_info("Mode A", false);
    T068_CHECK(!unselected.selected);
    T068_CHECK(unselected.checked == ui::SemanticCheckedState::Unchecked);
}

void toggle_value_contract() {
    const auto checked = ui::detail::toggle_semantic_info("Bypass", true);
    T068_CHECK(checked.role == ui::SemanticRole::Toggle);
    T068_CHECK(checked.name == "Bypass");
    T068_CHECK(checked.checked == ui::SemanticCheckedState::Checked);
    T068_CHECK(checked.focusable);
    T068_CHECK(checked.supports(ui::SemanticAction::Toggle));
    T068_CHECK(checked.supports(ui::SemanticAction::Focus));
    T068_CHECK(!checked.supports(ui::SemanticAction::Activate));

    const auto unchecked = ui::detail::toggle_semantic_info("Bypass", false);
    T068_CHECK(unchecked.checked == ui::SemanticCheckedState::Unchecked);
}

void slider_value_contract() {
    const auto info = ui::detail::slider_semantic_info(0.25f, -1.0f, 1.0f, 0.25f);
    T068_CHECK(info.role == ui::SemanticRole::Slider);
    T068_CHECK(info.numeric_value.has_value());
    T068_CHECK(*info.numeric_value == 0.25);
    T068_CHECK(info.value_range.has_value());
    T068_CHECK(info.value_range->minimum == -1.0);
    T068_CHECK(info.value_range->maximum == 1.0);
    T068_CHECK(info.value_range->step == 0.25);
    T068_CHECK(info.focusable);
    T068_CHECK(info.supports(ui::SemanticAction::Increment));
    T068_CHECK(info.supports(ui::SemanticAction::Decrement));
    T068_CHECK(info.supports(ui::SemanticAction::SetValue));
    T068_CHECK(info.supports(ui::SemanticAction::Focus));
    T068_CHECK(!info.supports(ui::SemanticAction::Toggle));

    const auto clamped = ui::detail::slider_semantic_info(5.0f, -1.0f, 1.0f, 0.25f);
    T068_CHECK(clamped.numeric_value.has_value());
    T068_CHECK(*clamped.numeric_value == 1.0);
}

void bounded_display_value_contract() {
    const auto progress = ui::detail::bounded_display_semantic_info(
        0.25f, 0.0f, 1.0f, false);
    T068_CHECK(progress.role == ui::SemanticRole::ProgressBar);
    T068_CHECK(progress.numeric_value.has_value());
    T068_CHECK(*progress.numeric_value == 0.25);
    T068_CHECK(progress.value_range.has_value());
    T068_CHECK(progress.value_range->minimum == 0.0);
    T068_CHECK(progress.value_range->maximum == 1.0);
    T068_CHECK(progress.value_range->step == 0.0);
    T068_CHECK(!progress.focusable);
    T068_CHECK(progress.actions.empty());

    const auto meter = ui::detail::bounded_display_semantic_info(
        4.0f, -1.0f, 1.0f, true);
    T068_CHECK(meter.role == ui::SemanticRole::Meter);
    T068_CHECK(meter.numeric_value.has_value());
    T068_CHECK(*meter.numeric_value == 1.0);
    T068_CHECK(meter.value_range.has_value());
    T068_CHECK(meter.value_range->minimum == -1.0);
    T068_CHECK(meter.value_range->maximum == 1.0);
    T068_CHECK(meter.actions.empty());

    const auto non_finite = ui::detail::bounded_display_semantic_info(
        std::numeric_limits<float>::quiet_NaN(), -2.0f, 2.0f, false);
    T068_CHECK(non_finite.numeric_value.has_value());
    T068_CHECK(*non_finite.numeric_value == -2.0);
}

void text_edit_value_contract() {
    const auto single_line = ui::detail::text_edit_semantic_info(
        "Name", "Ada", false);
    T068_CHECK(single_line.role == ui::SemanticRole::TextInput);
    T068_CHECK(single_line.name == "Name");
    T068_CHECK(single_line.text_value.has_value());
    T068_CHECK(*single_line.text_value == "Ada");
    T068_CHECK(single_line.focusable);
    T068_CHECK(single_line.supports(ui::SemanticAction::SetValue));
    T068_CHECK(single_line.supports(ui::SemanticAction::Focus));

    const auto multiline = ui::detail::text_edit_semantic_info(
        "Notes", "line one\nline two", true);
    T068_CHECK(multiline.role == ui::SemanticRole::TextArea);
    T068_CHECK(multiline.name == "Notes");
    T068_CHECK(multiline.text_value.has_value());
    T068_CHECK(*multiline.text_value == "line one\nline two");
    T068_CHECK(multiline.focusable);
    T068_CHECK(multiline.supports(ui::SemanticAction::SetValue));
    T068_CHECK(multiline.supports(ui::SemanticAction::Focus));
}

void combo_box_value_contract() {
    const auto collapsed = ui::detail::combo_box_semantic_info("Mode A", false);
    T068_CHECK(collapsed.role == ui::SemanticRole::ComboBox);
    T068_CHECK(collapsed.text_value.has_value());
    T068_CHECK(*collapsed.text_value == "Mode A");
    T068_CHECK(collapsed.expanded == ui::SemanticExpandedState::Collapsed);
    T068_CHECK(collapsed.focusable);
    T068_CHECK(collapsed.supports(ui::SemanticAction::Expand));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Collapse));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Select));
    T068_CHECK(collapsed.supports(ui::SemanticAction::Focus));

    const auto expanded = ui::detail::combo_box_semantic_info("Mode B", true);
    T068_CHECK(expanded.text_value.has_value());
    T068_CHECK(*expanded.text_value == "Mode B");
    T068_CHECK(expanded.expanded == ui::SemanticExpandedState::Expanded);
}

void dialog_value_contract() {
    const auto info = ui::detail::dialog_semantic_info("Settings");
    T068_CHECK(info.role == ui::SemanticRole::Dialog);
    T068_CHECK(info.name == "Settings");
    T068_CHECK(info.focusable);
    T068_CHECK(info.supports(ui::SemanticAction::Focus));
    T068_CHECK(!info.supports(ui::SemanticAction::Activate));
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
    T068_CHECK(info.role == ui::SemanticRole::TextInput);
    T068_CHECK(info.name == "Display name");
    T068_CHECK(info.text_value.has_value());
    T068_CHECK(*info.text_value == "Ada");
    T068_CHECK(info.focusable);
    T068_CHECK(!info.read_only);
    T068_CHECK(info.supports(ui::SemanticAction::SetValue));
    T068_CHECK(info.supports(ui::SemanticAction::Focus));
    T068_CHECK(!info.supports(ui::SemanticAction::Toggle));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::TextInput);
    T068_CHECK(node != nullptr);
    T068_CHECK(node->info.text_value.has_value());
    T068_CHECK(*node->info.text_value == "Ada");

    const ui::detail::SemanticIdentity identity{node_id, std::nullopt};
    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"Grace\nHopper"};
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, set_value));
    T068_CHECK(value.get() == "Grace Hopper");

    // Semantic writes keep the widget's max-length policy.
    set_value.text_value = std::string{"0123456789abcdef"};
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, set_value));
    T068_CHECK(value.get() == "0123456789ab");

    const auto live = ui::detail::SemanticTreeActionAccess::current_semantics(
        tree, identity);
    T068_CHECK(live.has_value());
    T068_CHECK(live->text_value.has_value());
    T068_CHECK(*live->text_value == "0123456789ab");

    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Increment)));
    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, identity, action_request(ui::SemanticAction::Toggle)));
}

void text_area_component_projection_contract() {
    ui::State<std::string> value{"line one"};
    auto root = ui::compile(ui::make_spec(ui::TextArea{"Notes", value}));
    const auto node_id = static_cast<ui::SemanticId>(root->id);

    const auto info = root->component->semantics();
    T068_CHECK(info.role == ui::SemanticRole::TextArea);
    T068_CHECK(info.name == "Notes");
    T068_CHECK(info.text_value.has_value());
    T068_CHECK(*info.text_value == "line one");
    T068_CHECK(info.focusable);
    T068_CHECK(info.supports(ui::SemanticAction::SetValue));
    T068_CHECK(info.supports(ui::SemanticAction::Focus));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(
        *root_ptr, ui::kInvalidNodeId);
    const auto* node = find_role(snapshot, ui::SemanticRole::TextArea);
    T068_CHECK(node != nullptr);
    T068_CHECK(node->info.name == "Notes");

    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"line one\nline two"};
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{node_id, std::nullopt}, set_value));
    T068_CHECK(value.get() == "line one\nline two");
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
    T068_CHECK(node != nullptr);
    T068_CHECK(node->info.read_only);
    T068_CHECK(node->info.enabled);
    T068_CHECK(node->info.focusable);
    T068_CHECK(node->info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    auto set_value = action_request(ui::SemanticAction::SetValue);
    set_value.text_value = std::string{"Grace"};
    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{text_id, std::nullopt}, set_value));
    T068_CHECK(value.get() == "Ada");
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
    T068_CHECK(node != nullptr);
    T068_CHECK(!node->info.enabled);
    T068_CHECK(!node->info.focusable);
    T068_CHECK(node->info.actions.empty());

    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{slider_id, std::nullopt},
        action_request(ui::SemanticAction::Increment)));
    T068_CHECK(near(gain.get(), 0.25f));
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
    T068_CHECK(snapshot.root == static_cast<ui::SemanticId>(root_ptr->id));
    const auto& tabs = snapshot.nodes.front();
    T068_CHECK(tabs.info.role == ui::SemanticRole::Tabs);
    T068_CHECK(tabs.info.focusable);
    T068_CHECK(tabs.info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    // The selected TabPanel is visible; collapsed panels are absent while the
    // three Tab nodes stay in logical order.
    const auto roles = child_roles(snapshot, tabs);
    T068_CHECK(roles == std::vector<ui::SemanticRole>({
        ui::SemanticRole::Tab,
        ui::SemanticRole::Tab,
        ui::SemanticRole::TabPanel,
        ui::SemanticRole::Tab,
    }));

    const auto* one = find_named_role(snapshot, ui::SemanticRole::Tab, "One");
    const auto* two = find_named_role(snapshot, ui::SemanticRole::Tab, "Two");
    const auto* three = find_named_role(snapshot, ui::SemanticRole::Tab, "Three");
    T068_CHECK(one != nullptr);
    T068_CHECK(two != nullptr);
    T068_CHECK(three != nullptr);
    T068_CHECK(!one->info.selected);
    T068_CHECK(two->info.selected);
    T068_CHECK(two->info.enabled);
    T068_CHECK(two->info.focusable);
    T068_CHECK(two->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Select, ui::SemanticAction::Focus}));
    T068_CHECK(!three->info.enabled);
    T068_CHECK(three->info.actions.empty());

    const auto* panel = find_role(snapshot, ui::SemanticRole::TabPanel);
    T068_CHECK(panel != nullptr);
    T068_CHECK(panel->info.enabled);
    T068_CHECK(!panel->info.focusable);
    T068_CHECK(panel->info.actions.empty());

    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{one->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    T068_CHECK(selected.get() == 1);

    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{three->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    T068_CHECK(selected.get() == 1);
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
    T068_CHECK(initial.has_value());
    const auto* tabs_initial = find_role(*initial, ui::SemanticRole::Tabs);
    const auto* button_initial = find_role(*initial, ui::SemanticRole::Button);
    T068_CHECK(tabs_initial != nullptr);
    T068_CHECK(button_initial != nullptr);
    T068_CHECK(button_initial->info.focused);
    T068_CHECK(!tabs_initial->info.focused);

    const auto* tab = find_named_role(*initial, ui::SemanticRole::Tab, "Two");
    T068_CHECK(tab != nullptr);
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{tab->id, std::nullopt},
        action_request(ui::SemanticAction::Focus)));

    const auto focused = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    T068_CHECK(focused.has_value());
    const auto* tabs_focused = find_role(*focused, ui::SemanticRole::Tabs);
    const auto* button_focused = find_role(*focused, ui::SemanticRole::Button);
    T068_CHECK(tabs_focused != nullptr);
    T068_CHECK(button_focused != nullptr);
    T068_CHECK(tabs_focused->info.focused);
    T068_CHECK(!button_focused->info.focused);
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
    T068_CHECK(snapshot.root == static_cast<ui::SemanticId>(root_ptr->id));
    const auto* list = find_role(snapshot, ui::SemanticRole::ListView);
    T068_CHECK(list != nullptr);
    T068_CHECK(list->info.focusable);
    T068_CHECK(list->info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    T068_CHECK(child_roles(snapshot, *list) == std::vector<ui::SemanticRole>({
        ui::SemanticRole::ListItem,
        ui::SemanticRole::ListItem,
        ui::SemanticRole::ListItem,
    }));

    const auto* one = find_named_role(snapshot, ui::SemanticRole::ListItem, "one");
    const auto* two = find_named_role(snapshot, ui::SemanticRole::ListItem, "two");
    const auto* three = find_named_role(snapshot, ui::SemanticRole::ListItem, "three");
    T068_CHECK(one != nullptr);
    T068_CHECK(two != nullptr);
    T068_CHECK(three != nullptr);
    T068_CHECK(one->info.enabled);
    T068_CHECK(!one->info.selected);
    T068_CHECK(one->info.focusable);
    T068_CHECK(one->info.actions == std::vector<ui::SemanticAction>({
        ui::SemanticAction::Select,
        ui::SemanticAction::Focus,
        ui::SemanticAction::Activate,
    }));
    T068_CHECK(!three->info.enabled);
    T068_CHECK(three->info.actions.empty());

    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{two->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    T068_CHECK(selected.get().has_value());
    T068_CHECK(*selected.get() == "two");
    T068_CHECK(activations == 0);

    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{one->id, std::nullopt},
        action_request(ui::SemanticAction::Activate)));
    T068_CHECK(activations == 1);
    T068_CHECK(activated_key == "one");
    T068_CHECK(*selected.get() == "one");

    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, ui::detail::SemanticIdentity{three->id, std::nullopt},
        action_request(ui::SemanticAction::Select)));
    T068_CHECK(*selected.get() == "one");
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
    T068_CHECK(snapshot.nodes.size() == 2);
    const auto handles = nodes_with_role(snapshot, ui::SemanticRole::RangeSliderHandle);
    T068_CHECK(handles.size() == 2);
    const auto* lower = handles[0];
    const auto* upper = handles[1];
    T068_CHECK(lower->info.numeric_value.has_value());
    T068_CHECK(near(static_cast<float>(*lower->info.numeric_value), 0.2f));
    T068_CHECK(upper->info.numeric_value.has_value());
    T068_CHECK(near(static_cast<float>(*upper->info.numeric_value), 0.8f));
    for (const auto* handle : handles) {
        T068_CHECK(handle->info.focusable);
        T068_CHECK(handle->info.value_range.has_value());
        T068_CHECK(handle->info.value_range->minimum == 0.0);
        T068_CHECK(handle->info.value_range->maximum == 1.0);
        T068_CHECK(near(static_cast<float>(handle->info.value_range->step), 0.1f));
        T068_CHECK(handle->info.actions == std::vector<ui::SemanticAction>({
            ui::SemanticAction::Increment,
            ui::SemanticAction::Decrement,
            ui::SemanticAction::SetValue,
            ui::SemanticAction::Focus,
        }));
    }

    const ui::detail::SemanticIdentity lower_identity{lower->id, std::nullopt};
    const ui::detail::SemanticIdentity upper_identity{upper->id, std::nullopt};

    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Increment)));
    T068_CHECK(near(range.get().low, 0.3f));
    T068_CHECK(near(range.get().high, 0.8f));

    auto set_upper = action_request(ui::SemanticAction::SetValue);
    set_upper.numeric_value = 0.9;
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, set_upper));
    T068_CHECK(near(range.get().high, 0.9f));

    // The lower handle cannot cross the upper handle's current value.
    auto set_lower = action_request(ui::SemanticAction::SetValue);
    set_lower.numeric_value = 0.95;
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, set_lower));
    T068_CHECK(near(range.get().low, 0.9f));

    // The upper handle cannot cross the lower handle either.
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, action_request(ui::SemanticAction::Decrement)));
    T068_CHECK(near(range.get().high, 0.9f));

    // Once the lower handle moves down the upper handle can follow.
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Decrement)));
    T068_CHECK(near(range.get().low, 0.8f));
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, upper_identity, action_request(ui::SemanticAction::Decrement)));
    T068_CHECK(near(range.get().high, 0.8f));
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
    T068_CHECK(handles.size() == 2);
    for (const auto* handle : handles) {
        T068_CHECK(handle->info.read_only);
        T068_CHECK(handle->info.focusable);
        T068_CHECK(handle->info.actions ==
                   std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    }

    const ui::detail::SemanticIdentity lower_identity{handles[0]->id, std::nullopt};
    T068_CHECK(!ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Increment)));
    T068_CHECK(near(range.get().low, 0.2f));

    read_only.set(false);
    tree.refresh_focus(platform);
    T068_CHECK(ui::detail::SemanticTreeActionAccess::dispatch_semantic_action(
        tree, lower_identity, action_request(ui::SemanticAction::Focus)));
    const auto focused = ui::detail::SemanticTreeActionAccess::build_snapshot(tree);
    T068_CHECK(focused.has_value());
    const auto* button = find_role(*focused, ui::SemanticRole::Button);
    T068_CHECK(button != nullptr);
    T068_CHECK(!button->info.focused);
}

void dialog_component_projection_contract() {
    ui::detail::DialogPanelLayout layout;
    layout.body_index = 0;
    ui::detail::DialogPanelComponent panel{layout, nullptr, nullptr, "Settings"};

    const auto info = panel.semantics();
    T068_CHECK(info.role == ui::SemanticRole::Dialog);
    T068_CHECK(info.name == "Settings");
    T068_CHECK(info.focusable);
    T068_CHECK(info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
    T068_CHECK(!info.supports(ui::SemanticAction::Activate));
}

void popup_menu_component_projection_contract() {
    std::vector<ui::PopupMenuItem> items;
    items.push_back(ui::PopupMenuItem::action("Open", [] {}));
    items.push_back(ui::PopupMenuItem::separator());
    items.push_back(ui::PopupMenuItem::action("Disabled", [] {}, false));

    auto root = ui::compile(ui::make_spec(ui::PopupMenu{"Actions", items}));
    const auto info = root->component->semantics();
    T068_CHECK(info.role == ui::SemanticRole::PopupMenu);
    T068_CHECK(info.name == "Actions");
    T068_CHECK(info.focusable);
    T068_CHECK(info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    auto session = std::make_shared<ui::detail::MenuPopupSession>();
    session->items = items;
    session->highlighted = 0;

    ui::detail::MenuPopupComponent popup{session};
    const auto popup_info = popup.semantics();
    T068_CHECK(popup_info.role == ui::SemanticRole::PopupMenu);
    T068_CHECK(popup_info.focusable);
    T068_CHECK(popup_info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});

    ui::detail::MenuPopupItemComponent open_item{session, 0};
    const auto open_info = open_item.semantics();
    T068_CHECK(open_info.role == ui::SemanticRole::MenuItem);
    T068_CHECK(open_info.name == "Open");
    T068_CHECK(open_info.enabled);
    T068_CHECK(open_info.selected);
    T068_CHECK(!open_info.focusable);
    T068_CHECK(open_info.actions ==
               std::vector<ui::SemanticAction>{ui::SemanticAction::Activate});

    ui::detail::MenuPopupItemComponent separator_item{session, 1};
    T068_CHECK(separator_item.semantics().role == ui::SemanticRole::None);

    ui::detail::MenuPopupItemComponent disabled_item{session, 2};
    const auto disabled_info = disabled_item.semantics();
    T068_CHECK(disabled_info.role == ui::SemanticRole::MenuItem);
    T068_CHECK(disabled_info.name == "Disabled");
    T068_CHECK(!disabled_info.enabled);
    T068_CHECK(!disabled_info.selected);
    T068_CHECK(disabled_info.actions.empty());

    std::vector<ui::ChildPlacement> placements(3);
    popup.layout_children({0.0f, 0.0f, 120.0f, 60.0f}, {}, placements);
    T068_CHECK(placements[0].bounds.h > 0.0f);
    T068_CHECK(placements[1].bounds.y >= placements[0].bounds.y + placements[0].bounds.h);
    T068_CHECK(placements[2].bounds.y >= placements[1].bounds.y + placements[1].bounds.h);
}

} // namespace

int main() {
    try {
        label_value_contract();
        checkbox_value_contract();
        radio_value_contract();
        toggle_value_contract();
        slider_value_contract();
        bounded_display_value_contract();
        text_edit_value_contract();
        combo_box_value_contract();
        dialog_value_contract();
        text_input_component_projection_contract();
        text_area_component_projection_contract();
        semantic_value_writes_revalidate_read_only();
        disabled_widget_projection_removes_actions();
        tabs_component_projection_contract();
        tabs_focus_action_targets_the_composite_owner();
        list_view_component_projection_contract();
        range_slider_component_projection_contract();
        range_slider_read_only_and_focus_contract();
        dialog_component_projection_contract();
        popup_menu_component_projection_contract();
        std::cout << "PASS t068 widget semantic values\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 widget semantic values: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}