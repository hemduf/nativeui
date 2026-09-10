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

enum class OverlayPlacement {
    Below,
    Above,
    Right,
    Left,
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

    bool operator==(const OverlayHandle&) const noexcept = default;

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
