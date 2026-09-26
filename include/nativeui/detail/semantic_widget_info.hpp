#pragma once

#include <nativeui/semantics.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <type_traits>

namespace ui::detail {

[[nodiscard]] inline SemanticInfo label_semantic_info(std::string_view text) {
    SemanticInfo info;
    info.role = SemanticRole::Text;
    info.name = text;
    return info;
}

[[nodiscard]] inline SemanticInfo checkbox_semantic_info(
    std::string_view label,
    bool checked) {
    SemanticInfo info;
    info.role = SemanticRole::Checkbox;
    info.name = label;
    info.checked = checked ? SemanticCheckedState::Checked
                           : SemanticCheckedState::Unchecked;
    info.focusable = true;
    info.actions = {SemanticAction::Toggle, SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo radio_button_semantic_info(
    std::string_view label,
    bool selected) {
    SemanticInfo info;
    info.role = SemanticRole::RadioButton;
    info.name = label;
    info.selected = selected;
    info.checked = selected ? SemanticCheckedState::Checked
                            : SemanticCheckedState::Unchecked;
    info.focusable = true;
    info.actions = {SemanticAction::Select, SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo toggle_semantic_info(
    std::string_view label,
    bool checked) {
    SemanticInfo info;
    info.role = SemanticRole::Toggle;
    info.name = label;
    info.checked = checked ? SemanticCheckedState::Checked
                           : SemanticCheckedState::Unchecked;
    info.focusable = true;
    info.actions = {SemanticAction::Toggle, SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo slider_semantic_info(
    float value,
    float minimum,
    float maximum,
    float step) {
    SemanticInfo info;
    info.role = SemanticRole::Slider;
    info.numeric_value = static_cast<double>(std::clamp(value, minimum, maximum));
    info.value_range = SemanticValueRange{
        static_cast<double>(minimum),
        static_cast<double>(maximum),
        static_cast<double>(step),
    };
    info.focusable = true;
    info.actions = {
        SemanticAction::Increment,
        SemanticAction::Decrement,
        SemanticAction::SetValue,
        SemanticAction::Focus,
    };
    return info;
}

[[nodiscard]] inline SemanticInfo bounded_display_semantic_info(
    float value,
    float minimum,
    float maximum,
    bool meter) {
    SemanticInfo info;
    info.role = meter ? SemanticRole::Meter : SemanticRole::ProgressBar;
    const float effective = std::isfinite(value)
        ? std::clamp(value, minimum, maximum)
        : minimum;
    info.numeric_value = static_cast<double>(effective);
    info.value_range = SemanticValueRange{
        static_cast<double>(minimum),
        static_cast<double>(maximum),
        0.0,
    };
    return info;
}

[[nodiscard]] inline SemanticInfo text_edit_semantic_info(
    std::string_view label,
    std::string_view text,
    bool multiline) {
    SemanticInfo info;
    info.role = multiline ? SemanticRole::TextArea : SemanticRole::TextInput;
    info.name = label;
    info.text_value = std::string{text};
    info.focusable = true;
    info.actions = {SemanticAction::SetValue, SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo combo_box_semantic_info(
    std::string_view selected_label,
    bool expanded) {
    SemanticInfo info;
    info.role = SemanticRole::ComboBox;
    info.text_value = std::string{selected_label};
    info.expanded = expanded ? SemanticExpandedState::Expanded
                             : SemanticExpandedState::Collapsed;
    info.focusable = true;
    info.actions = {
        SemanticAction::Expand,
        SemanticAction::Collapse,
        SemanticAction::Select,
        SemanticAction::Focus,
    };
    return info;
}

[[nodiscard]] inline SemanticInfo dialog_semantic_info(std::string_view title) {
    SemanticInfo info;
    info.role = SemanticRole::Dialog;
    info.name = title;
    info.focusable = true;
    info.actions = {SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo list_view_semantic_info() {
    SemanticInfo info;
    info.role = SemanticRole::ListView;
    info.focusable = true;
    info.actions = {SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo list_item_semantic_info(
    std::string_view name,
    bool selected,
    bool activatable) {
    SemanticInfo info;
    info.role = SemanticRole::ListItem;
    info.name = name;
    info.selected = selected;
    info.focusable = true;
    info.actions = {SemanticAction::Select, SemanticAction::Focus};
    if (activatable) info.actions.push_back(SemanticAction::Activate);
    return info;
}

/// Best-effort accessible name for an ordinary ListView row key. Key types that
/// can be viewed as text keep their exact logical value; arithmetic keys are
/// rendered deterministically. Other key types contribute no invented name and
/// rely on the row's own exposed content.
template <class Key>
[[nodiscard]] inline std::string list_item_key_name(const Key& key) {
    if constexpr (std::is_convertible_v<const Key&, std::string_view>) {
        return std::string{std::string_view{key}};
    } else if constexpr (std::is_arithmetic_v<Key>) {
        return std::to_string(key);
    } else {
        return {};
    }
}

[[nodiscard]] inline SemanticInfo tabs_semantic_info() {
    SemanticInfo info;
    info.role = SemanticRole::Tabs;
    info.focusable = true;
    info.actions = {SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo tab_semantic_info(
    std::string_view label,
    bool selected) {
    SemanticInfo info;
    info.role = SemanticRole::Tab;
    info.name = label;
    info.selected = selected;
    info.focusable = true;
    info.actions = {SemanticAction::Select, SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo tab_panel_semantic_info() {
    SemanticInfo info;
    info.role = SemanticRole::TabPanel;
    return info;
}

[[nodiscard]] inline SemanticInfo popup_menu_semantic_info() {
    SemanticInfo info;
    info.role = SemanticRole::PopupMenu;
    info.focusable = true;
    info.actions = {SemanticAction::Focus};
    return info;
}

[[nodiscard]] inline SemanticInfo menu_item_semantic_info(
    std::string_view label,
    bool enabled,
    bool selected,
    bool activatable) {
    SemanticInfo info;
    info.role = SemanticRole::MenuItem;
    info.name = label;
    info.enabled = enabled;
    info.selected = selected;
    if (enabled && activatable) info.actions = {SemanticAction::Activate};
    return info;
}

[[nodiscard]] inline SemanticInfo range_slider_handle_semantic_info(
    float value,
    float minimum,
    float maximum,
    float step) {
    SemanticInfo info;
    info.role = SemanticRole::RangeSliderHandle;
    info.numeric_value = static_cast<double>(std::clamp(value, minimum, maximum));
    info.value_range = SemanticValueRange{
        static_cast<double>(minimum),
        static_cast<double>(maximum),
        static_cast<double>(step),
    };
    info.focusable = true;
    info.actions = {
        SemanticAction::Increment,
        SemanticAction::Decrement,
        SemanticAction::SetValue,
        SemanticAction::Focus,
    };
    return info;
}

} // namespace ui::detail
