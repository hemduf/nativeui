#pragma once

#include <nativeui/component_base.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace ui {

enum class OverlayMode {
    NonModal,
    Modal,
};

enum class OverlayPointerPolicy {
    Normal,
    Ignore,
};

// X11/Xlib exposes process-wide preprocessor macros named `Above` and `Below`.
// NativeUI public headers must remain consumable after Xlib headers, so the
// anchor-relative names deliberately avoid those unqualifiable macro tokens.
enum class OverlayPlacement {
    AnchorBelow,
    AnchorAbove,
    AnchorRight,
    AnchorLeft,
    Center,
    Auto,
};

namespace detail {
struct OverlayOwnerToken final {};
} // namespace detail

class OverlayHandle {
public:
    OverlayHandle() = default;

    [[nodiscard]] bool valid() const noexcept {
        return id_ != 0 && !owner_.expired();
    }

    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] bool operator==(const OverlayHandle& other) const noexcept {
        return id_ == other.id_ && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }

private:
    friend class UI;

    OverlayHandle(std::weak_ptr<const detail::OverlayOwnerToken> owner,
                  std::uint64_t id) noexcept
        : owner_(std::move(owner)), id_(id) {}

    std::weak_ptr<const detail::OverlayOwnerToken> owner_;
    std::uint64_t id_{};
};

struct OverlaySpec {
    OverlayMode mode{OverlayMode::NonModal};
    OverlayPointerPolicy pointer_policy{OverlayPointerPolicy::Normal};
    std::optional<NodeId> anchor;
    OverlayPlacement placement{OverlayPlacement::Auto};
    bool dismiss_on_escape{};
    bool dismiss_on_outside_pointer_down{};
    Spec content;
};

} // namespace ui
