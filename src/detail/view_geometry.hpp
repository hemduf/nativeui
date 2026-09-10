#pragma once

#include <nativeui/geometry.hpp>

#include <cmath>
#include <optional>
#include <utility>

namespace ui::detail {

inline constexpr float kPreferredSizeEpsilon = 0.0001f;

[[nodiscard]] inline bool valid_scale(float scale) noexcept {
    return std::isfinite(scale) && scale > 0.0f;
}

[[nodiscard]] inline bool valid_logical_size(Size size) noexcept {
    return std::isfinite(size.w) && std::isfinite(size.h) && size.w > 0.0f && size.h > 0.0f;
}

[[nodiscard]] inline bool valid_preferred_size(Size size) noexcept {
    return std::isfinite(size.w) && std::isfinite(size.h) && size.w >= 0.0f && size.h >= 0.0f;
}

[[nodiscard]] inline bool valid_physical_extent(Size size) noexcept {
    return std::isfinite(size.w) && std::isfinite(size.h) && size.w > 0.0f && size.h > 0.0f;
}

[[nodiscard]] inline float retain_last_valid_scale(float reported, float last_valid) noexcept {
    return valid_scale(reported) ? reported : (valid_scale(last_valid) ? last_valid : 1.0f);
}

[[nodiscard]] inline Size logical_to_physical_size(Size logical, float scale) noexcept {
    const float safe_scale = retain_last_valid_scale(scale, 1.0f);
    return Size{std::ceil(logical.w * safe_scale), std::ceil(logical.h * safe_scale)};
}

[[nodiscard]] inline Size physical_to_logical_size(Size physical, float scale) noexcept {
    const float safe_scale = retain_last_valid_scale(scale, 1.0f);
    return Size{physical.w / safe_scale, physical.h / safe_scale};
}

[[nodiscard]] inline Point physical_to_logical_point(Point physical, float scale) noexcept {
    const float safe_scale = retain_last_valid_scale(scale, 1.0f);
    return Point{physical.x / safe_scale, physical.y / safe_scale};
}

[[nodiscard]] inline Rect logical_to_physical_covering_rect(Rect logical, float scale) noexcept {
    const float safe_scale = retain_last_valid_scale(scale, 1.0f);
    const float left = std::floor(logical.x * safe_scale);
    const float top = std::floor(logical.y * safe_scale);
    const float right = std::ceil((logical.x + logical.w) * safe_scale);
    const float bottom = std::ceil((logical.y + logical.h) * safe_scale);
    return Rect{left,
                top,
                std::max(0.0f, right - left),
                std::max(0.0f, bottom - top)};
}

class ViewGeometryState final {
public:
    explicit ViewGeometryState(Size initial_logical) noexcept
        : logical_size_(valid_logical_size(initial_logical) ? initial_logical : Size{}) {}

    [[nodiscard]] float last_valid_scale() const noexcept { return last_valid_scale_; }
    [[nodiscard]] Size logical_size() const noexcept { return logical_size_; }
    [[nodiscard]] Size physical_size() const noexcept { return physical_size_; }
    [[nodiscard]] bool renderable() const noexcept { return renderable_; }
    [[nodiscard]] const std::optional<Size>& pending_request() const noexcept {
        return pending_request_;
    }

    /// Apply one authoritative native configure snapshot. A valid reported
    /// scale is adopted even if the physical extent is transiently zero, but
    /// zero/non-finite extents preserve the last valid logical viewport and do
    /// not produce a layout size.
    [[nodiscard]] std::optional<Size> configure(Size physical, float reported_scale) noexcept {
        last_valid_scale_ = retain_last_valid_scale(reported_scale, last_valid_scale_);
        if (!valid_physical_extent(physical)) {
            renderable_ = false;
            return std::nullopt;
        }

        physical_size_ = physical;
        renderable_ = true;
        logical_size_ = physical_to_logical_size(physical_size_, last_valid_scale_);
        pending_request_.reset();
        return logical_size_;
    }

    /// Convert a public logical request without mutating authoritative size
    /// state. The caller records the pending request only after the native API
    /// has accepted the physical request.
    [[nodiscard]] std::optional<Size> physical_request(Size logical) const noexcept {
        if (!valid_logical_size(logical)) return std::nullopt;
        return logical_to_physical_size(logical, last_valid_scale_);
    }

    void record_successful_request(Size logical) noexcept {
        if (valid_logical_size(logical)) pending_request_ = logical;
    }

private:
    float last_valid_scale_{1.0f};
    Size logical_size_{};
    Size physical_size_{};
    bool renderable_{true};
    std::optional<Size> pending_request_;
};

class PreferredSizeState final {
public:
    void queue(Size preferred) noexcept {
        if (!valid_preferred_size(preferred)) return;
        pending_ = preferred;
    }

    void reset() noexcept {
        pending_.reset();
        last_notified_.reset();
    }

    [[nodiscard]] bool dispatching() const noexcept { return dispatching_; }

    template <class Callback>
    bool dispatch_once(Callback&& callback) {
        if (dispatching_ || !pending_) return false;

        const Size next = *pending_;
        pending_.reset();
        if (last_notified_ && !differs(next, *last_notified_)) return false;

        // Publish the current value before entering user code. A synchronous
        // queue of the same value is therefore coalesced, while a new value is
        // retained for the next safe checkpoint rather than recursively fired.
        last_notified_ = next;
        dispatching_ = true;
        struct DispatchGuard final {
            bool& flag;
            ~DispatchGuard() { flag = false; }
        } guard{dispatching_};
        std::forward<Callback>(callback)(next);
        return true;
    }

private:
    [[nodiscard]] static bool differs(Size a, Size b) noexcept {
        return std::fabs(a.w - b.w) > kPreferredSizeEpsilon ||
               std::fabs(a.h - b.h) > kPreferredSizeEpsilon;
    }

    bool dispatching_{};
    std::optional<Size> pending_;
    std::optional<Size> last_notified_;
};

} // namespace ui::detail
