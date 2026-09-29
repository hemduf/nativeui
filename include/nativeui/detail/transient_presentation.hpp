#pragma once

namespace ui::detail {

/// Optional retained-policy hook for per-UI transient presentations whose
/// lifetime must not survive a global dismissal gesture.
///
/// The owning Tree traverses mounted components and invokes this hook for
/// PointerDown anywhere in the UI, Escape, a new T061 overlay request and view
/// deactivation. Tooltip uses it to cancel its T065 timer and close its
/// non-hit-test overlay without introducing a global "current tooltip" or a
/// second per-UI registry. The hook must be reentrancy-safe and idempotent.
class TransientPresentation {
public:
    virtual ~TransientPresentation() = default;
    virtual void dismiss_transient_presentation() = 0;
};

} // namespace ui::detail
