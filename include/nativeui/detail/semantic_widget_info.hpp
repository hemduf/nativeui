#pragma once

#include <nativeui/semantics.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

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

} // namespace ui::detail
