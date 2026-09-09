#pragma once

namespace ui::detail {

// Internal retained-tree contract for roving focus groups such as RadioButton.
// Group identity is owned by the consumer/widget binding; the tree never
// registers process-global names or mutable group state.
class FocusGroupParticipant {
public:
    virtual ~FocusGroupParticipant() = default;

    [[nodiscard]] virtual const void* focus_group_identity() const noexcept = 0;
    [[nodiscard]] virtual bool focus_group_selected() const noexcept = 0;
    virtual void focus_group_select() = 0;
};

} // namespace ui::detail
