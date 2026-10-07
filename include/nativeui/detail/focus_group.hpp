#pragma once

#include <nativeui/input.hpp>
#include <functional>

namespace ui::detail {

// Internal retained-tree contract for roving focus groups such as RadioButton.
// Group identity is owned by the consumer/widget binding; the tree never
// registers process-global names or mutable group state.
class FocusGroupParticipant {
public:
    virtual ~FocusGroupParticipant() = default;

    [[nodiscard]] virtual const void* focus_group_identity() const noexcept = 0;
    [[nodiscard]] virtual bool focus_group_selected() const = 0;
    virtual void focus_group_select() = 0;
    [[nodiscard]] virtual std::uint64_t focus_group_selection_revision() const noexcept { return 0; }
    // Owned cache update at the focus commit; no application callback.
    virtual void focus_group_did_focus() noexcept {}
    // A roving input has its own activation lifetime, separate from mounting.
    virtual void focus_group_select_guarded(const std::function<bool()>& allowed) {
        if (!allowed || allowed()) focus_group_select();
    }
    [[nodiscard]] virtual bool focus_group_accepts_navigation_key(Key) const noexcept { return true; }
    /// Boundary selection is opt-in so disclosure headers can handle Home/End
    /// as focus-only commands through their own retained input policy.
    [[nodiscard]] virtual bool focus_group_accepts_boundary_key(Key) const noexcept { return false; }
};

} // namespace ui::detail
