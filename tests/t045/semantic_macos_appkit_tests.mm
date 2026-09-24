#include "../../src/detail/semantic_macos_appkit.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

using NativeRole = ui::detail::MacOSAccessibilityRole;
using Subrole = ui::detail::MacOSAccessibilitySubrole;
using NativePublicationState = ui::detail::SemanticNativePublicationState;
using NativeProxyState = ui::detail::MacOSAccessibilityProxyState;

struct ExpectedRole final {
    NativeRole token;
    NSString* appkit_role;
};

const std::array<ExpectedRole, 18> expected_roles{{
    {NativeRole::Button, NSAccessibilityButtonRole},
    {NativeRole::CheckBox, NSAccessibilityCheckBoxRole},
    {NativeRole::RadioButton, NSAccessibilityRadioButtonRole},
    {NativeRole::Slider, NSAccessibilitySliderRole},
    {NativeRole::ProgressIndicator, NSAccessibilityProgressIndicatorRole},
    {NativeRole::LevelIndicator, NSAccessibilityLevelIndicatorRole},
    {NativeRole::StaticText, NSAccessibilityStaticTextRole},
    {NativeRole::TextField, NSAccessibilityTextFieldRole},
    {NativeRole::TextArea, NSAccessibilityTextAreaRole},
    {NativeRole::ComboBox, NSAccessibilityComboBoxRole},
    {NativeRole::Menu, NSAccessibilityMenuRole},
    {NativeRole::MenuItem, NSAccessibilityMenuItemRole},
    {NativeRole::List, NSAccessibilityListRole},
    {NativeRole::Row, NSAccessibilityRowRole},
    {NativeRole::TabGroup, NSAccessibilityTabGroupRole},
    {NativeRole::Group, NSAccessibilityGroupRole},
    {NativeRole::Window, NSAccessibilityWindowRole},
    {NativeRole::Image, NSAccessibilityImageRole},
}};

void fixed_role_tokens_translate_to_appkit() {
    for (const auto& expected : expected_roles) {
        NSString* actual =
            ui::detail::macos_accessibility_appkit_role(expected.token);
        CHECK(actual != nil);
        CHECK([actual isEqualToString:expected.appkit_role]);
    }
}

void fixed_subrole_tokens_translate_to_appkit() {
    CHECK(ui::detail::macos_accessibility_appkit_subrole(Subrole::None) == nil);

    NSString* tab_button =
        ui::detail::macos_accessibility_appkit_subrole(Subrole::TabButton);
    CHECK(tab_button != nil);
    CHECK([tab_button isEqualToString:NSAccessibilityTabButtonSubrole]);

    NSString* dialog =
        ui::detail::macos_accessibility_appkit_subrole(Subrole::Dialog);
    CHECK(dialog != nil);
    CHECK([dialog isEqualToString:NSAccessibilityDialogSubrole]);
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    ui::SemanticId node_id,
    std::string name,
    ui::SemanticRole role = ui::SemanticRole::Button,
    std::optional<std::string> text_value = std::nullopt,
    std::optional<double> numeric_value = std::nullopt,
    ui::SemanticCheckedState checked = ui::SemanticCheckedState::NotApplicable) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = node_id;

    ui::SemanticNodeSnapshot node;
    node.id = node_id;
    node.info.role = role;
    node.info.name = std::move(name);
    node.info.text_value = std::move(text_value);
    node.info.numeric_value = numeric_value;
    node.info.checked = checked;
    node.bounds = {1.0f, 2.0f, 30.0f, 12.0f};
    snapshot->nodes.push_back(std::move(node));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_state_snapshot(
    std::uint64_t generation,
    ui::SemanticId node_id,
    std::string description,
    bool enabled,
    bool focused) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = node_id;

    ui::SemanticNodeSnapshot node;
    node.id = node_id;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = "stateful control";
    node.info.description = std::move(description);
    node.info.enabled = enabled;
    node.info.focusable = true;
    node.info.focused = focused;
    node.bounds = {1.0f, 2.0f, 30.0f, 12.0f};
    snapshot->nodes.push_back(std::move(node));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_list_snapshot(
    std::uint64_t generation,
    ui::SemanticId list_id,
    ui::VirtualSemanticItemToken token,
    bool include_item) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = list_id;

    ui::SemanticNodeSnapshot list;
    list.id = list_id;
    list.info.role = ui::SemanticRole::ListView;
    list.bounds = {4.0f, 5.0f, 100.0f, 40.0f};

    if (include_item) {
        auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
        ui::VirtualSemanticItemMetadata item;
        item.token = token;
        item.name = "logical row";
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        metadata->push_back(std::move(item));

        auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
        token_index->emplace(token, 0U);

        list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
            1U,
            std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
            std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
            token,
            list.bounds,
            20.0f,
            0.0f);
    }

    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

void accessibility_proxy_state_reads_current_generation_and_fails_closed() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 41U;

    auto proxy = NativeProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(proxy.has_value());
    CHECK(!proxy->read().has_value());
    CHECK(!NativeProxyState::ordinary(
        publication_state.reader_source(), ui::kInvalidSemanticId).has_value());

    const ui::detail::SemanticNativeGeometry first_geometry{
        2.0f, {-120.0f, 64.0f}};
    auto first_batch = publication_state.publish(
        ordinary_snapshot(1U, node_id, "first"),
        {ui::SemanticChange::StructureChanged},
        first_geometry);
    CHECK(first_batch.has_value());

    auto first_read = proxy->read();
    CHECK(first_read.has_value());
    CHECK(first_read->generation() == first_batch->generation());
    CHECK(first_read->semantic_generation() == 1U);
    CHECK(first_read->info().name == "first");
    CHECK(first_read->geometry() == first_geometry);

    const ui::detail::SemanticNativeGeometry second_geometry{
        1.5f, {320.0f, 180.0f}};
    auto second_batch = publication_state.publish(
        ordinary_snapshot(2U, node_id, "second"),
        {ui::SemanticChange::ValueChanged},
        second_geometry);
    CHECK(second_batch.has_value());

    auto second_read = proxy->read();
    CHECK(second_read.has_value());
    CHECK(second_read->generation() == second_batch->generation());
    CHECK(second_read->semantic_generation() == 2U);
    CHECK(second_read->info().name == "second");
    CHECK(second_read->geometry() == second_geometry);

    // A callback that already retained generation 1 remains self-contained.
    CHECK(first_read->semantic_generation() == 1U);
    CHECK(first_read->info().name == "first");
    CHECK(first_read->geometry() == first_geometry);

    publication_state.shutdown();
    CHECK(!proxy->read().has_value());
    CHECK(first_read->info().name == "first");
}

void accessibility_proxy_read_projects_one_atomic_generation() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 55U;

    auto proxy = NativeProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(proxy.has_value());
    CHECK(!ui::detail::macos_accessibility_proxy_read(*proxy).has_value());

    const ui::detail::SemanticNativeGeometry first_geometry{
        2.0f, {-120.0f, 64.0f}};
    auto first_batch = publication_state.publish(
        ordinary_snapshot(
            7U,
            node_id,
            "gain",
            ui::SemanticRole::Slider,
            std::nullopt,
            0.25),
        {ui::SemanticChange::StructureChanged},
        first_geometry);
    CHECK(first_batch.has_value());

    auto first = ui::detail::macos_accessibility_proxy_read(*proxy);
    CHECK(first.has_value());
    CHECK(first->generation() == first_batch->generation());
    CHECK(first->semantic_generation() == 7U);
    CHECK(first->node_id() == node_id);
    CHECK(!first->virtual_token().has_value());
    CHECK(first->role_mapping().role == NativeRole::Slider);
    CHECK(first->role_mapping().subrole == Subrole::None);
    CHECK(first->info().name == "gain");
    CHECK(first->info().numeric_value == 0.25);
    CHECK(first->geometry() == first_geometry);

    const auto first_bounds = first->physical_screen_bounds();
    CHECK(first_bounds.x == -118.0f);
    CHECK(first_bounds.y == 68.0f);
    CHECK(first_bounds.w == 60.0f);
    CHECK(first_bounds.h == 24.0f);

    const ui::detail::SemanticNativeGeometry second_geometry{
        1.5f, {320.0f, 180.0f}};
    auto second_batch = publication_state.publish(
        ordinary_snapshot(
            8U,
            node_id,
            "gain newer",
            ui::SemanticRole::Slider,
            std::nullopt,
            0.75),
        {ui::SemanticChange::ValueChanged, ui::SemanticChange::BoundsChanged},
        second_geometry);
    CHECK(second_batch.has_value());

    auto second = ui::detail::macos_accessibility_proxy_read(*proxy);
    CHECK(second.has_value());
    CHECK(second->generation() == second_batch->generation());
    CHECK(second->semantic_generation() == 8U);
    CHECK(second->info().name == "gain newer");
    CHECK(second->info().numeric_value == 0.75);
    CHECK(second->geometry() == second_geometry);

    const auto second_bounds = second->physical_screen_bounds();
    CHECK(second_bounds.x == 321.0f);
    CHECK(second_bounds.y == 183.0f);
    CHECK(second_bounds.w == 46.0f);
    CHECK(second_bounds.h == 18.0f);

    // A callback-local read never mixes later semantic data or geometry into
    // the generation it already retained.
    CHECK(first->semantic_generation() == 7U);
    CHECK(first->info().name == "gain");
    CHECK(first->info().numeric_value == 0.25);
    CHECK(first->physical_screen_bounds().x == -118.0f);

    auto flattened = publication_state.publish(
        ordinary_snapshot(
            9U,
            node_id,
            "flattened",
            ui::SemanticRole::None),
        {ui::SemanticChange::StructureChanged},
        second_geometry);
    CHECK(flattened.has_value());
    CHECK(!ui::detail::macos_accessibility_proxy_read(*proxy).has_value());

    // The old immutable callback view remains complete after the current node
    // becomes a flattened role with no native proxy.
    CHECK(first->role_mapping().role == NativeRole::Slider);
    CHECK(first->info().name == "gain");
}

void accessibility_virtual_proxy_state_uses_stable_token_and_becomes_defunct() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId list_id = 77U;
    constexpr ui::VirtualSemanticItemToken token = 9001U;

    auto proxy = NativeProxyState::virtual_item(
        publication_state.reader_source(), list_id, token);
    CHECK(proxy.has_value());
    CHECK(proxy->node_id() == list_id);
    CHECK(proxy->virtual_token().has_value());
    CHECK(*proxy->virtual_token() == token);
    CHECK(!NativeProxyState::virtual_item(
        publication_state.reader_source(), list_id,
        ui::kInvalidVirtualSemanticItemToken).has_value());

    auto present = publication_state.publish(
        virtual_list_snapshot(1U, list_id, token, true),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(present.has_value());

    auto read = proxy->read();
    CHECK(read.has_value());
    CHECK(read->virtual_token().has_value());
    CHECK(*read->virtual_token() == token);
    CHECK(read->info().name == "logical row");

    auto projected = ui::detail::macos_accessibility_proxy_read(*proxy);
    CHECK(projected.has_value());
    CHECK(projected->virtual_token().has_value());
    CHECK(*projected->virtual_token() == token);
    CHECK(projected->role_mapping().role == NativeRole::Row);
    CHECK(projected->info().name == "logical row");

    auto removed = publication_state.publish(
        virtual_list_snapshot(2U, list_id, token, false),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(removed.has_value());
    CHECK(!proxy->read().has_value());
    CHECK(!ui::detail::macos_accessibility_proxy_read(*proxy).has_value());

    // The in-flight immutable read remains valid after the current dataset drops
    // the token; no live row or retained component is needed to finish it.
    CHECK(read->info().name == "logical row");
    CHECK(projected->info().name == "logical row");
}

Class test_anchor_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

void accessibility_proxy_runtime_class_is_consumer_scoped() {
    Class first_anchor = test_anchor_class(
        "NUI_semantic_appkit_first_111111111111_PuglWrapperView");
    Class second_anchor = test_anchor_class(
        "NUI_semantic_appkit_second_222222222222_PuglWrapperView");

    Class first =
        ui::detail::macos_accessibility_appkit_proxy_class(first_anchor);
    Class first_again =
        ui::detail::macos_accessibility_appkit_proxy_class(first_anchor);
    Class second =
        ui::detail::macos_accessibility_appkit_proxy_class(second_anchor);

    CHECK(first != Nil);
    CHECK(first_again == first);
    CHECK(second != Nil);
    CHECK(second != first);
    CHECK(class_getSuperclass(first) == [NSAccessibilityElement class]);
    CHECK(class_getSuperclass(second) == [NSAccessibilityElement class]);
    CHECK(class_getInstanceVariable(
        first, ui::detail::kMacOSAccessibilityProxyStateIvar) != nullptr);
    CHECK(class_getInstanceVariable(
        second, ui::detail::kMacOSAccessibilityProxyStateIvar) != nullptr);

    const std::string first_expected =
        std::string{class_getName(first_anchor)} +
        "_NativeUIAccessibilityElement";
    const std::string second_expected =
        std::string{class_getName(second_anchor)} +
        "_NativeUIAccessibilityElement";
    CHECK(first_expected == class_getName(first));
    CHECK(second_expected == class_getName(second));
}

void accessibility_proxy_instance_reads_current_snapshot_and_fails_closed() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 91U;

    auto first_batch = publication_state.publish(
        ordinary_snapshot(
            10U,
            node_id,
            "gain",
            ui::SemanticRole::Slider,
            std::nullopt,
            0.25),
        {ui::SemanticChange::StructureChanged},
        {2.0f, {-120.0f, 64.0f}});
    CHECK(first_batch.has_value());

    auto state = NativeProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(state.has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_instance_333333333333_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);
    CHECK(object_getClass(element) ==
          ui::detail::macos_accessibility_appkit_proxy_class(anchor));
    CHECK([element isAccessibilityElement] == YES);
    CHECK([[element accessibilityRole] isEqualToString:NSAccessibilitySliderRole]);
    CHECK([element accessibilitySubrole] == nil);
    CHECK([[element accessibilityLabel] isEqualToString:@"gain"]);

    id value = [element accessibilityValue];
    CHECK([value isKindOfClass:[NSNumber class]]);
    CHECK([(NSNumber*)value doubleValue] == 0.25);

    auto second_batch = publication_state.publish(
        ordinary_snapshot(
            11U,
            node_id,
            "gain newer",
            ui::SemanticRole::Slider,
            std::nullopt,
            0.75),
        {ui::SemanticChange::ValueChanged},
        {1.5f, {320.0f, 180.0f}});
    CHECK(second_batch.has_value());
    CHECK([element isAccessibilityElement] == YES);
    CHECK([[element accessibilityRole] isEqualToString:NSAccessibilitySliderRole]);
    CHECK([[element accessibilityLabel] isEqualToString:@"gain newer"]);
    value = [element accessibilityValue];
    CHECK([value isKindOfClass:[NSNumber class]]);
    CHECK([(NSNumber*)value doubleValue] == 0.75);

    publication_state.shutdown();
    CHECK([element isAccessibilityElement] == NO);
    CHECK([element accessibilityRole] == nil);
    CHECK([element accessibilitySubrole] == nil);
    CHECK([element accessibilityLabel] == nil);
    CHECK([element accessibilityValue] == nil);
}

void accessibility_proxy_projects_help_enabled_and_focus_state() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 93U;

    CHECK(publication_state.publish(
        ordinary_state_snapshot(1U, node_id, "first help", true, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto state = NativeProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(state.has_value());
    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_state_555555555555_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);
    CHECK([[element accessibilityHelp] isEqualToString:@"first help"]);
    CHECK([element isAccessibilityEnabled] == YES);
    CHECK([element isAccessibilityFocused] == YES);

    CHECK(publication_state.publish(
        ordinary_state_snapshot(2U, node_id, "second help", false, false),
        {ui::SemanticChange::FocusChanged, ui::SemanticChange::ValueChanged},
        {}).has_value());
    CHECK([[element accessibilityHelp] isEqualToString:@"second help"]);
    CHECK([element isAccessibilityEnabled] == NO);
    CHECK([element isAccessibilityFocused] == NO);

    publication_state.shutdown();
    CHECK([element accessibilityHelp] == nil);
    CHECK([element isAccessibilityEnabled] == NO);
    CHECK([element isAccessibilityFocused] == NO);
}

void accessibility_proxy_value_projects_checked_state() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 92U;
    auto batch = publication_state.publish(
        ordinary_snapshot(
            1U,
            node_id,
            "enabled",
            ui::SemanticRole::Checkbox,
            std::nullopt,
            std::nullopt,
            ui::SemanticCheckedState::Mixed),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(batch.has_value());

    auto state = NativeProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(state.has_value());
    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_checked_444444444444_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);
    id value = [element accessibilityValue];
    CHECK([value isKindOfClass:[NSNumber class]]);
    CHECK([(NSNumber*)value integerValue] == 2);
}

struct RecordedSemanticAction final {
    ui::detail::SemanticIdentity identity;
    ui::detail::SemanticActionRequest request;
};

class RecordingSemanticActionTarget final
    : public ui::detail::SemanticActionTarget {
public:
    RecordingSemanticActionTarget(
        ui::detail::SemanticIdentity expected_identity,
        ui::SemanticInfo current_info,
        std::shared_ptr<std::vector<RecordedSemanticAction>> records)
        : expected_identity_(expected_identity),
          current_info_(std::move(current_info)),
          records_(std::move(records)) {}

    [[nodiscard]] std::optional<ui::SemanticInfo> current_semantics(
        const ui::detail::SemanticIdentity& identity) const override {
        return identity == expected_identity_
            ? std::optional<ui::SemanticInfo>{current_info_}
            : std::nullopt;
    }

    bool dispatch_semantic_action(
        const ui::detail::SemanticIdentity& identity,
        const ui::detail::SemanticActionRequest& request) override {
        if (identity != expected_identity_) {
            return false;
        }
        records_->push_back({identity, request});
        return true;
    }

private:
    ui::detail::SemanticIdentity expected_identity_;
    ui::SemanticInfo current_info_;
    std::shared_ptr<std::vector<RecordedSemanticAction>> records_;
};

std::shared_ptr<const ui::SemanticTreeSnapshot> actionable_snapshot(
    std::uint64_t generation,
    ui::SemanticId node_id) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = node_id;

    ui::SemanticNodeSnapshot node;
    node.id = node_id;
    node.info.role = ui::SemanticRole::Slider;
    node.info.name = "action target";
    node.info.enabled = true;
    node.info.focusable = true;
    node.info.numeric_value = 0.25;
    node.info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.01};
    node.info.actions = {
        ui::SemanticAction::Activate,
        ui::SemanticAction::Focus,
        ui::SemanticAction::Increment,
        ui::SemanticAction::Decrement,
        ui::SemanticAction::SetValue,
        ui::SemanticAction::Select,
        ui::SemanticAction::Expand,
        ui::SemanticAction::Collapse,
    };
    snapshot->nodes.push_back(std::move(node));
    return snapshot;
}

void accessibility_proxy_actions_route_only_through_the_view_endpoint() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 94U;
    const auto snapshot = actionable_snapshot(1U, node_id);
    CHECK(publication_state.publish(
        snapshot,
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    CHECK(!publisher->publish(*snapshot).empty());

    ui::detail::DispatcherOwner dispatcher_owner;
    auto records = std::make_shared<std::vector<RecordedSemanticAction>>();
    const ui::detail::SemanticIdentity identity{node_id, std::nullopt};
    auto target = std::make_shared<RecordingSemanticActionTarget>(
        identity, snapshot->nodes.front().info, records);
    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target, publisher};
    target.reset();

    auto state = NativeProxyState::ordinary(
        publication_state.reader_source(),
        node_id,
        {},
        binding.endpoint());
    CHECK(state.has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_actions_666666666666_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(accessibilityPerformPress)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(accessibilityPerformIncrement)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(accessibilityPerformDecrement)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilityFocused:)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilitySelected:)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilityExpanded:)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilityValue:)] == YES);

    CHECK([element accessibilityPerformPress] == YES);
    CHECK(records->empty());
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Activate);

    CHECK([element accessibilityPerformIncrement] == YES);
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Increment);

    CHECK([element accessibilityPerformDecrement] == YES);
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Decrement);

    [element setAccessibilityFocused:YES];
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Focus);
    const std::size_t after_focus = records->size();
    [element setAccessibilityFocused:NO];
    CHECK(dispatcher_owner.checkpoint() == 0U);
    CHECK(records->size() == after_focus);

    [element setAccessibilitySelected:YES];
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Select);
    const std::size_t after_selection = records->size();
    [element setAccessibilitySelected:NO];
    CHECK(dispatcher_owner.checkpoint() == 0U);
    CHECK(records->size() == after_selection);

    [element setAccessibilityExpanded:YES];
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Expand);
    [element setAccessibilityExpanded:NO];
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::Collapse);

    [element setAccessibilityValue:@0.75];
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->back().request.action == ui::SemanticAction::SetValue);
    CHECK(records->back().request.numeric_value == 0.75);
    CHECK(!records->back().request.text_value.has_value());

    const std::size_t after_value = records->size();
    [element setAccessibilityValue:@"invalid"];
    CHECK(dispatcher_owner.checkpoint() == 0U);
    CHECK(records->size() == after_value);

    binding.reset();
    CHECK([element accessibilityPerformPress] == NO);
    [element setAccessibilityExpanded:YES];
    CHECK(dispatcher_owner.checkpoint() == 0U);
    CHECK(records->size() == after_value);
}

void accessibility_virtual_proxy_action_preserves_logical_identity() {
    NativePublicationState publication_state;
    constexpr ui::SemanticId list_id = 95U;
    constexpr ui::VirtualSemanticItemToken token = 8123U;
    const auto snapshot = virtual_list_snapshot(1U, list_id, token, true);
    CHECK(publication_state.publish(
        snapshot,
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    CHECK(!publisher->publish(*snapshot).empty());

    ui::SemanticInfo live_info;
    live_info.role = ui::SemanticRole::ListItem;
    live_info.enabled = true;
    live_info.actions = {
        ui::SemanticAction::Select,
        ui::SemanticAction::Focus,
    };

    ui::detail::DispatcherOwner dispatcher_owner;
    auto records = std::make_shared<std::vector<RecordedSemanticAction>>();
    const ui::detail::SemanticIdentity identity{list_id, token};
    auto target = std::make_shared<RecordingSemanticActionTarget>(
        identity, live_info, records);
    ui::detail::SemanticActionViewBinding binding{
        dispatcher_owner.dispatcher(), target, publisher};
    target.reset();

    auto state = NativeProxyState::virtual_item(
        publication_state.reader_source(),
        list_id,
        token,
        {},
        binding.endpoint());
    CHECK(state.has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_virtual_actions_777777777777_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilitySelected:)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilityFocused:)] == YES);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(accessibilityPerformPress)] == NO);
    CHECK([element isAccessibilitySelectorAllowed:
        @selector(setAccessibilityValue:)] == NO);

    [element setAccessibilitySelected:YES];
    CHECK(records->empty());
    CHECK(dispatcher_owner.checkpoint() == 1U);
    CHECK(records->size() == 1U);
    CHECK(records->front().identity == identity);
    CHECK(records->front().request.action == ui::SemanticAction::Select);
}

void accessibility_proxy_runtime_class_rejects_unscoped_anchor() {
    Class unscoped = test_anchor_class("NativeUIAccessibilityUnscopedAnchor");
    CHECK(ui::detail::macos_accessibility_appkit_proxy_class(unscoped) == Nil);
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            fixed_role_tokens_translate_to_appkit();
            fixed_subrole_tokens_translate_to_appkit();
            accessibility_proxy_state_reads_current_generation_and_fails_closed();
            accessibility_proxy_read_projects_one_atomic_generation();
            accessibility_virtual_proxy_state_uses_stable_token_and_becomes_defunct();
            accessibility_proxy_runtime_class_is_consumer_scoped();
            accessibility_proxy_instance_reads_current_snapshot_and_fails_closed();
            accessibility_proxy_projects_help_enabled_and_focus_state();
            accessibility_proxy_value_projects_checked_state();
            accessibility_proxy_actions_route_only_through_the_view_endpoint();
            accessibility_virtual_proxy_action_preserves_logical_identity();
            accessibility_proxy_runtime_class_rejects_unscoped_anchor();
            std::cout << "PASS semantic macOS AppKit mapping\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL semantic macOS AppKit mapping: " << error.what()
                      << '\n';
            return EXIT_FAILURE;
        }
    }
}
