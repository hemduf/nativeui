#include <nativeui/detail/semantic_uia_mapping.hpp>
#include <nativeui/detail/semantic_widget_info.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("line " + std::to_string(__LINE__) +       \
                                     ": check failed: " #condition);            \
        }                                                                       \
    } while (false)

using ui::SemanticAction;
using ui::SemanticCheckedState;
using ui::SemanticInfo;
using ui::SemanticRole;
using ui::detail::semantic_uia_pattern_eligibility;
using ui::detail::semantic_uia_role_mapping;
using ui::detail::UiaControlType;
using ui::detail::uia_has_pattern;
using ui::detail::UiaPattern;
using ui::detail::UiaPatternEligibility;
using ui::detail::UiaRoleMapping;

struct ExpectedRoleMapping final {
    SemanticRole role;
    UiaControlType control_type;
    bool read_only_range_value{false};
    bool container_selection{false};
    bool item_container{false};
    bool text_access{false};
};

constexpr std::array expected_role_mappings{
    ExpectedRoleMapping{SemanticRole::Button, UiaControlType::Button},
    ExpectedRoleMapping{SemanticRole::Checkbox, UiaControlType::CheckBox},
    ExpectedRoleMapping{SemanticRole::RadioButton, UiaControlType::RadioButton},
    ExpectedRoleMapping{SemanticRole::Toggle, UiaControlType::CheckBox},
    ExpectedRoleMapping{SemanticRole::Slider, UiaControlType::Slider},
    ExpectedRoleMapping{SemanticRole::RangeSliderHandle, UiaControlType::Thumb},
    ExpectedRoleMapping{SemanticRole::ProgressBar, UiaControlType::ProgressBar,
                        true},
    ExpectedRoleMapping{SemanticRole::Meter, UiaControlType::ProgressBar, true},
    ExpectedRoleMapping{SemanticRole::Text, UiaControlType::Text, false, false,
                        false, true},
    ExpectedRoleMapping{SemanticRole::TextInput, UiaControlType::Edit, false,
                        false, false, true},
    ExpectedRoleMapping{SemanticRole::TextArea, UiaControlType::Edit, false,
                        false, false, true},
    ExpectedRoleMapping{SemanticRole::ComboBox, UiaControlType::ComboBox, false,
                        true},
    ExpectedRoleMapping{SemanticRole::PopupMenu, UiaControlType::Menu},
    ExpectedRoleMapping{SemanticRole::MenuItem, UiaControlType::MenuItem},
    ExpectedRoleMapping{SemanticRole::ListView, UiaControlType::List, false,
                        true, true},
    ExpectedRoleMapping{SemanticRole::ListItem, UiaControlType::ListItem},
    ExpectedRoleMapping{SemanticRole::Tabs, UiaControlType::Tab},
    ExpectedRoleMapping{SemanticRole::Tab, UiaControlType::TabItem},
    ExpectedRoleMapping{SemanticRole::TabPanel, UiaControlType::Group},
    ExpectedRoleMapping{SemanticRole::Dialog, UiaControlType::Window},
    ExpectedRoleMapping{SemanticRole::Group, UiaControlType::Group},
    ExpectedRoleMapping{SemanticRole::Image, UiaControlType::Image},
    ExpectedRoleMapping{SemanticRole::Custom, UiaControlType::Group},
};

[[nodiscard]] SemanticInfo button_info(bool enabled = true) {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = "button";
    info.enabled = enabled;
    info.focusable = true;
    info.actions = {SemanticAction::Activate, SemanticAction::Focus};
    return info;
}

void fixed_role_mapping_matches_the_frozen_contract() {
    CHECK(!semantic_uia_role_mapping(SemanticRole::None).has_value());

    for (const auto& expected : expected_role_mappings) {
        const auto actual = semantic_uia_role_mapping(expected.role);
        CHECK(actual.has_value());
        CHECK(actual->control_type == expected.control_type);
        CHECK(actual->read_only_range_value == expected.read_only_range_value);
        CHECK(actual->container_selection == expected.container_selection);
        CHECK(actual->item_container == expected.item_container);
        CHECK(actual->text_access == expected.text_access);
    }
}

void standard_widget_actions_select_their_primary_patterns() {
    const auto checkbox = semantic_uia_pattern_eligibility(
        ui::detail::checkbox_semantic_info("check", false));
    CHECK(checkbox.toggle);
    CHECK(!checkbox.invoke);
    CHECK(!checkbox.selection_item);
    CHECK(!checkbox.range_value);
    CHECK(!checkbox.value);
    CHECK(!checkbox.text);

    const auto toggle =
        semantic_uia_pattern_eligibility(ui::detail::toggle_semantic_info("t", true));
    CHECK(toggle.toggle);
    CHECK(!toggle.selection_item);

    const auto radio = semantic_uia_pattern_eligibility(
        ui::detail::radio_button_semantic_info("r", true));
    CHECK(radio.selection_item);
    CHECK(!radio.toggle);

    const auto button = semantic_uia_pattern_eligibility(button_info());
    CHECK(button.invoke);
    CHECK(!button.toggle);
    CHECK(!button.selection_item);
    CHECK(!button.text);

    const auto slider = semantic_uia_pattern_eligibility(
        ui::detail::slider_semantic_info(0.5f, 0.0f, 1.0f, 0.1f));
    CHECK(slider.range_value);
    CHECK(slider.range_value_writable);
    CHECK(!slider.invoke);
    CHECK(!slider.value);

    auto read_only_slider =
        ui::detail::slider_semantic_info(0.5f, 0.0f, 1.0f, 0.1f);
    read_only_slider.read_only = true;
    const auto read_only = semantic_uia_pattern_eligibility(read_only_slider);
    CHECK(read_only.range_value);
    CHECK(!read_only.range_value_writable);

    const auto handle = semantic_uia_pattern_eligibility(
        ui::detail::range_slider_handle_semantic_info(4.0f, 0.0f, 10.0f, 0.5f));
    CHECK(handle.range_value);
    CHECK(handle.range_value_writable);

    const auto progress = semantic_uia_pattern_eligibility(
        ui::detail::bounded_display_semantic_info(
            0.5f, 0.0f, 1.0f, /*meter=*/false));
    CHECK(progress.range_value);
    CHECK(!progress.range_value_writable);
    CHECK(!progress.invoke);
    CHECK(!progress.toggle);

    const auto meter = semantic_uia_pattern_eligibility(
        ui::detail::bounded_display_semantic_info(
            0.5f, 0.0f, 1.0f, /*meter=*/true));
    CHECK(meter.range_value);
    CHECK(!meter.range_value_writable);

    const auto label =
        semantic_uia_pattern_eligibility(ui::detail::label_semantic_info("text"));
    CHECK(label.text);
    CHECK(!label.value);
    CHECK(!label.range_value);
    CHECK(!label.invoke);

    const auto input = semantic_uia_pattern_eligibility(
        ui::detail::text_edit_semantic_info("label", "value", /*multiline=*/false));
    CHECK(input.value);
    CHECK(input.text);
    CHECK(!input.range_value);
    CHECK(!input.invoke);

    const auto area = semantic_uia_pattern_eligibility(
        ui::detail::text_edit_semantic_info("label", "value", /*multiline=*/true));
    CHECK(area.value);
    CHECK(area.text);

    const auto combo = semantic_uia_pattern_eligibility(
        ui::detail::combo_box_semantic_info("selected", true));
    CHECK(combo.expand_collapse);
    CHECK(combo.selection);
    CHECK(!combo.value);
    CHECK(!combo.selection_item);

    const auto popup =
        semantic_uia_pattern_eligibility(ui::detail::popup_menu_semantic_info());
    CHECK(!popup.invoke);
    CHECK(!popup.selection_item);
    CHECK(!popup.expand_collapse);

    const auto menu_item_activatable = semantic_uia_pattern_eligibility(
        ui::detail::menu_item_semantic_info("item", true, false, true));
    CHECK(menu_item_activatable.invoke);
    CHECK(!menu_item_activatable.selection_item);

    SemanticInfo menu_item_selected =
        ui::detail::menu_item_semantic_info("item", true, true, false);
    menu_item_selected.actions = {SemanticAction::Select};
    const auto menu_item_selectable =
        semantic_uia_pattern_eligibility(menu_item_selected);
    CHECK(menu_item_selectable.selection_item);
    CHECK(!menu_item_selectable.invoke);

    const auto list =
        semantic_uia_pattern_eligibility(ui::detail::list_view_semantic_info());
    CHECK(list.selection);
    CHECK(list.item_container);
    CHECK(!list.virtualized_item);
    CHECK(!list.invoke);

    const auto item =
        semantic_uia_pattern_eligibility(ui::detail::list_item_semantic_info(
            "row", false, false));
    CHECK(item.selection_item);
    CHECK(!item.virtualized_item);
    CHECK(!item.item_container);

    const auto virtual_item = semantic_uia_pattern_eligibility(
        ui::detail::list_item_semantic_info("row", false, false),
        /*virtual_item=*/true);
    CHECK(virtual_item.selection_item);
    CHECK(virtual_item.virtualized_item);

    const auto tabs = semantic_uia_pattern_eligibility(
        ui::detail::tabs_semantic_info());
    CHECK(!tabs.selection_item);
    CHECK(!tabs.item_container);

    const auto tab =
        semantic_uia_pattern_eligibility(ui::detail::tab_semantic_info("tab", true));
    CHECK(tab.selection_item);
    CHECK(!tab.toggle);

    const auto panel =
        semantic_uia_pattern_eligibility(ui::detail::tab_panel_semantic_info());
    CHECK(!panel.invoke);
    CHECK(!panel.selection_item);

    const auto dialog =
        semantic_uia_pattern_eligibility(ui::detail::dialog_semantic_info("dialog"));
    CHECK(!dialog.invoke);
    CHECK(!dialog.text);

    SemanticInfo group;
    group.role = SemanticRole::Group;
    group.name = "group";
    const auto group_eligibility = semantic_uia_pattern_eligibility(group);
    CHECK(!group_eligibility.invoke);
    CHECK(!group_eligibility.toggle);
    CHECK(!group_eligibility.selection);
}

void disabled_nodes_still_advertise_their_patterns() {
    // Pattern capability follows the advertised T045 action set. The adapter
    // reports IsEnabled=false and every mutation is revalidated (and rejected)
    // on the UI thread, so a disabled control must not silently lose its
    // discoverable pattern.
    const auto disabled = semantic_uia_pattern_eligibility(button_info(false));
    CHECK(disabled.invoke);
}

void role_mandated_read_only_patterns_do_not_require_actions() {
    // §8 requires read-only RangeValue for ProgressBar/Meter, Selection and
    // ItemContainer for ListView, and text access for the text roles even
    // though those read patterns have no corresponding mutating T045 action.
    const auto progress = semantic_uia_pattern_eligibility(
        ui::detail::bounded_display_semantic_info(0.5f, 0.0f, 1.0f, false));
    CHECK(progress.range_value);
    CHECK(!progress.range_value_writable);

    const auto list =
        semantic_uia_pattern_eligibility(ui::detail::list_view_semantic_info());
    CHECK(list.selection);
    CHECK(list.item_container);

    const auto label =
        semantic_uia_pattern_eligibility(ui::detail::label_semantic_info("x"));
    CHECK(label.text);
}

void custom_roles_project_only_their_advertised_actions() {
    SemanticInfo custom;
    custom.role = SemanticRole::Custom;

    CHECK(semantic_uia_pattern_eligibility(custom).invoke == false);
    CHECK(semantic_uia_pattern_eligibility(custom).range_value == false);
    CHECK(semantic_uia_pattern_eligibility(custom).value == false);

    custom.actions = {SemanticAction::Activate};
    CHECK(semantic_uia_pattern_eligibility(custom).invoke);

    custom.actions = {SemanticAction::Toggle};
    CHECK(semantic_uia_pattern_eligibility(custom).toggle);

    custom.actions = {SemanticAction::Select};
    CHECK(semantic_uia_pattern_eligibility(custom).selection_item);

    custom.actions = {SemanticAction::Expand, SemanticAction::Collapse};
    CHECK(semantic_uia_pattern_eligibility(custom).expand_collapse);

    custom.actions = {SemanticAction::SetValue};
    custom.text_value = "text";
    const auto text_value = semantic_uia_pattern_eligibility(custom);
    CHECK(text_value.value);
    CHECK(!text_value.range_value);

    custom.text_value.reset();
    custom.numeric_value = 0.5;
    const auto numeric_value = semantic_uia_pattern_eligibility(custom);
    CHECK(numeric_value.range_value);
    CHECK(numeric_value.range_value_writable);
    CHECK(!numeric_value.value);

    // An ambiguous or absent value domain must not invent a pattern.
    custom.numeric_value.reset();
    const auto no_domain = semantic_uia_pattern_eligibility(custom);
    CHECK(!no_domain.value);
    CHECK(!no_domain.range_value);

    custom.numeric_value = 0.5;
    custom.text_value = "ambiguous";
    const auto ambiguous = semantic_uia_pattern_eligibility(custom);
    CHECK(!ambiguous.value);
    CHECK(!ambiguous.range_value);
}

void pattern_query_agrees_with_the_eligibility_value() {
    const auto info = ui::detail::slider_semantic_info(0.5f, 0.0f, 1.0f, 0.1f);
    const auto eligibility = semantic_uia_pattern_eligibility(info);

    CHECK(uia_has_pattern(eligibility, UiaPattern::RangeValue));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::Invoke));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::Toggle));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::SelectionItem));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::ExpandCollapse));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::Value));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::Text));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::Selection));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::ItemContainer));
    CHECK(!uia_has_pattern(eligibility, UiaPattern::VirtualizedItem));

    const auto list =
        semantic_uia_pattern_eligibility(ui::detail::list_view_semantic_info());
    CHECK(uia_has_pattern(list, UiaPattern::Selection));
    CHECK(uia_has_pattern(list, UiaPattern::ItemContainer));

    const auto virtual_item = semantic_uia_pattern_eligibility(
        ui::detail::list_item_semantic_info("row", false, false), true);
    CHECK(uia_has_pattern(virtual_item, UiaPattern::VirtualizedItem));
}

void flattened_roles_never_map_to_a_control_type() {
    // Every closed role except the flattened None has a native control type;
    // an out-of-range value never invents one.
    for (const auto& expected : expected_role_mappings) {
        CHECK(semantic_uia_role_mapping(expected.role).has_value());
    }
    CHECK(!semantic_uia_role_mapping(SemanticRole::None).has_value());
    CHECK(!semantic_uia_role_mapping(static_cast<SemanticRole>(255)).has_value());
}

} // namespace

int main() {
    try {
        fixed_role_mapping_matches_the_frozen_contract();
        standard_widget_actions_select_their_primary_patterns();
        disabled_nodes_still_advertise_their_patterns();
        role_mandated_read_only_patterns_do_not_require_actions();
        custom_roles_project_only_their_advertised_actions();
        pattern_query_agrees_with_the_eligibility_value();
        flattened_roles_never_map_to_a_control_type();
        std::cout << "PASS semantic UIA mapping\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic UIA mapping: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
