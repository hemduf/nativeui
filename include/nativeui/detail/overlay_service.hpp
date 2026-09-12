#pragma once

#include <nativeui/overlay.hpp>

namespace ui::detail {

/// Per-UI T061 overlay seam handed to retained components through MountContext.
///
/// Components such as Tooltip need to create and close anchor-tracking overlays
/// from a T065 timer callback rather than from an input dispatch. The service
/// is owned by the concrete UI and never escapes the owning retained tree; it
/// exposes no OverlayState, Tree or platform object. Ordinary application and
/// widget overlay requests keep using UI::show_overlay/OverlayCommandSource so
/// their dismissal policy stays centralized.
class OverlayService {
public:
    virtual ~OverlayService() = default;

    [[nodiscard]] virtual OverlayHandle present(OverlaySpec overlay) = 0;
    [[nodiscard]] virtual bool dismiss(OverlayHandle handle) = 0;
};

} // namespace ui::detail
