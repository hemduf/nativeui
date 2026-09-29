#pragma once

namespace ui::detail {

// Restores a callback-frame-local borrowed pointer/flag pair on every exit path.
// The destructor performs only no-throw scalar assignments and never invokes
// application code, making it safe for foreign-callback unwind containment.
template <typename Borrowed>
class ScopedBorrowState final {
public:
    ScopedBorrowState(Borrowed& borrowed_slot, bool& decision_slot, Borrowed current) noexcept
        : borrowed_slot_(borrowed_slot),
          decision_slot_(decision_slot),
          previous_borrowed_(borrowed_slot),
          previous_decision_(decision_slot) {
        borrowed_slot_ = current;
        decision_slot_ = false;
    }

    ~ScopedBorrowState() noexcept {
        borrowed_slot_ = previous_borrowed_;
        decision_slot_ = previous_decision_;
    }

    ScopedBorrowState(const ScopedBorrowState&) = delete;
    ScopedBorrowState& operator=(const ScopedBorrowState&) = delete;
    ScopedBorrowState(ScopedBorrowState&&) = delete;
    ScopedBorrowState& operator=(ScopedBorrowState&&) = delete;

private:
    Borrowed& borrowed_slot_;
    bool& decision_slot_;
    Borrowed previous_borrowed_;
    bool previous_decision_{};
};

} // namespace ui::detail
