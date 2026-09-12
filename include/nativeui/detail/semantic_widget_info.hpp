#pragma once

#include <nativeui/semantics.hpp>

#include <string_view>

namespace ui::detail {

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

} // namespace ui::detail
