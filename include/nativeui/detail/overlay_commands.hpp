#pragma once

#include <nativeui/overlay.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ui::detail {

enum class OverlayComponentCommandKind {
    Show,
    CloseThenInvoke,
};

// Component-to-UI overlay requests are drained only after Tree::dispatch has
// reached its structural safe checkpoint. This keeps widgets on the shared
// T061 overlay stack without giving components ownership of UI/Tree internals.
struct OverlayComponentCommand final {
    OverlayComponentCommandKind kind{OverlayComponentCommandKind::Show};
    OverlaySpec overlay;
    OverlayHandle handle;
    NodeId guard_anchor{kInvalidNodeId};
    bool suppress_when_anchor_read_only{};
    std::function<void(OverlayHandle)> on_shown;
    std::function<void()> after_close;

    [[nodiscard]] static OverlayComponentCommand show(
        OverlaySpec overlay,
        std::function<void(OverlayHandle)> on_shown = {}) {
        OverlayComponentCommand command;
        command.kind = OverlayComponentCommandKind::Show;
        command.overlay = std::move(overlay);
        command.on_shown = std::move(on_shown);
        return command;
    }

    [[nodiscard]] static OverlayComponentCommand close_then_invoke(
        OverlayHandle handle,
        NodeId guard_anchor,
        bool suppress_when_anchor_read_only,
        std::function<void()> after_close) {
        OverlayComponentCommand command;
        command.kind = OverlayComponentCommandKind::CloseThenInvoke;
        command.handle = std::move(handle);
        command.guard_anchor = guard_anchor;
        command.suppress_when_anchor_read_only = suppress_when_anchor_read_only;
        command.after_close = std::move(after_close);
        return command;
    }
};

class OverlayCommandSource {
public:
    virtual ~OverlayCommandSource() = default;
    [[nodiscard]] virtual std::optional<OverlayComponentCommand>
    take_overlay_command() = 0;
};

// Policy lives on the anchor component instead of in OverlaySpec so generic
// T061 overlays keep their existing public contract. UI consults the current
// retained anchor by NodeId when applying T035-specific dismissal behavior.
class OverlayAnchorPolicy {
public:
    virtual ~OverlayAnchorPolicy() = default;
    [[nodiscard]] virtual bool dismiss_overlay_on_tab() const noexcept { return false; }
    [[nodiscard]] virtual bool dismiss_overlay_when_disabled() const noexcept { return true; }
    [[nodiscard]] virtual bool dismiss_overlay_when_read_only() const noexcept { return false; }
};

} // namespace ui::detail
