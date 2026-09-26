#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace ui::detail {

inline constexpr float kPreferredSizeEpsilon = 0.0001f;
inline constexpr float kPuglMaximumViewSpan = 10000.0f;

[[nodiscard]] inline unsigned physical_to_pugl_view_span(float physical) noexcept {
    if (!std::isfinite(physical)) {
        return physical > 0.0f ? static_cast<unsigned>(kPuglMaximumViewSpan) : 1U;
    }
    return static_cast<unsigned>(
        std::ceil(std::clamp(physical, 1.0f, kPuglMaximumViewSpan)));
}

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

[[nodiscard]] inline bool valid_physical_screen_origin(Point origin) noexcept {
    return std::isfinite(origin.x) && std::isfinite(origin.y);
}

[[nodiscard]] inline float retain_last_valid_scale(float reported, float last_valid) noexcept {
    return valid_scale(reported) ? reported : (valid_scale(last_valid) ? last_valid : 1.0f);
}

[[nodiscard]] inline Point retain_last_valid_physical_screen_origin(
    Point reported,
    Point last_valid) noexcept {
    return valid_physical_screen_origin(reported)
        ? reported
        : (valid_physical_screen_origin(last_valid) ? last_valid : Point{});
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

/// Convert one logical view-relative rectangle to physical screen coordinates.
/// The origin is already expressed in physical coordinates, so translation is
/// deliberately applied after the T043 scale/covering conversion and exactly
/// once.
[[nodiscard]] inline Rect logical_to_physical_screen_rect(
    Rect logical,
    float scale,
    Point physical_screen_origin) noexcept {
    auto physical = logical_to_physical_covering_rect(logical, scale);
    physical.x += physical_screen_origin.x;
    physical.y += physical_screen_origin.y;
    return physical;
}

/// Lifetime-detached source for the small T043 native geometry pair used by
/// retained semantic checkpoints. ViewGeometryState remains the sole authority:
/// valid UI-thread observations update this source in-place, while invalid
/// observations leave the last valid pair untouched.
///
/// A pump may retain this object across a re-entrant dispatcher drain without
/// retaining ViewCore or any Tree/Node/Component state. The source itself is
/// still UI-thread-confined; native readers receive only a copied immutable
/// SemanticNativeGeometry later at the publication boundary.
class ViewNativeGeometryCaptureState final {
public:
    [[nodiscard]] float last_valid_scale() const noexcept {
        return last_valid_scale_;
    }

    [[nodiscard]] Point physical_screen_origin() const noexcept {
        return physical_screen_origin_;
    }

private:
    friend class ViewGeometryState;

    float last_valid_scale_{1.0f};
    Point physical_screen_origin_{};
};

class ViewGeometryState final {
public:
    explicit ViewGeometryState(Size initial_logical) noexcept
        : logical_size_(valid_logical_size(initial_logical) ? initial_logical : Size{}) {}

    /// Geometry state keeps value semantics even after a semantic pump has
    /// retained a capture source. Copies receive the same current values but do
    /// not share the mutable per-view capture object.
    ViewGeometryState(const ViewGeometryState& other) noexcept
        : last_valid_scale_(other.last_valid_scale_),
          last_scale_observation_valid_(other.last_scale_observation_valid_),
          physical_screen_origin_(other.physical_screen_origin_),
          last_screen_origin_observation_valid_(other.last_screen_origin_observation_valid_),
          logical_size_(other.logical_size_),
          physical_size_(other.physical_size_),
          renderable_(other.renderable_),
          pending_request_(other.pending_request_) {}

    ViewGeometryState& operator=(const ViewGeometryState& other) noexcept {
        if (this == &other) return *this;

        last_valid_scale_ = other.last_valid_scale_;
        last_scale_observation_valid_ = other.last_scale_observation_valid_;
        physical_screen_origin_ = other.physical_screen_origin_;
        last_screen_origin_observation_valid_ = other.last_screen_origin_observation_valid_;
        logical_size_ = other.logical_size_;
        physical_size_ = other.physical_size_;
        renderable_ = other.renderable_;
        pending_request_ = other.pending_request_;
        sync_native_geometry_capture_state();
        return *this;
    }

    [[nodiscard]] float last_valid_scale() const noexcept { return last_valid_scale_; }
    [[nodiscard]] bool last_scale_observation_valid() const noexcept {
        return last_scale_observation_valid_;
    }
    [[nodiscard]] Point physical_screen_origin() const noexcept {
        return physical_screen_origin_;
    }
    [[nodiscard]] bool last_screen_origin_observation_valid() const noexcept {
        return last_screen_origin_observation_valid_;
    }
    [[nodiscard]] Size logical_size() const noexcept { return logical_size_; }
    [[nodiscard]] Size physical_size() const noexcept { return physical_size_; }
    [[nodiscard]] bool renderable() const noexcept { return renderable_; }
    [[nodiscard]] const std::optional<Size>& pending_request() const noexcept {
        return pending_request_;
    }

    /// Retain a lifetime-safe capture source for production semantic pumps.
    /// Allocation is intentionally lazy so non-accessibility ViewGeometryState
    /// users keep the existing no-allocation construction path. The returned
    /// object contains only the current scale/origin values and never retains
    /// this ViewGeometryState or its native/retained owner.
    [[nodiscard]] std::shared_ptr<const ViewNativeGeometryCaptureState>
    retain_native_geometry_capture_state() {
        if (!native_geometry_capture_state_) {
            auto state = std::make_shared<ViewNativeGeometryCaptureState>();
            state->last_valid_scale_ = last_valid_scale_;
            state->physical_screen_origin_ = physical_screen_origin_;
            native_geometry_capture_state_ = std::move(state);
        }
        return native_geometry_capture_state_;
    }

    [[nodiscard]] bool observe_scale(float reported_scale) noexcept {
        last_scale_observation_valid_ = valid_scale(reported_scale);
        if (!last_scale_observation_valid_) return false;
        last_valid_scale_ = reported_scale;
        sync_native_geometry_capture_state();
        return true;
    }

    /// Retain the most recent finite physical top-left screen origin for this
    /// view. Negative coordinates are valid on multi-monitor desktops. A
    /// transient invalid platform report fails closed and preserves the exact
    /// previous origin so semantic/native readers never publish NaN/Inf bounds.
    [[nodiscard]] bool observe_physical_screen_origin(Point reported_origin) noexcept {
        last_screen_origin_observation_valid_ = valid_physical_screen_origin(reported_origin);
        if (!last_screen_origin_observation_valid_) return false;
        physical_screen_origin_ = reported_origin;
        sync_native_geometry_capture_state();
        return true;
    }

    /// Apply one authoritative native configure snapshot. A valid reported
    /// scale is adopted even if the physical extent is transiently zero, but
    /// zero/non-finite extents preserve the last valid logical viewport and do
    /// not produce a layout size. The most recent scale-observation validity is
    /// retained so the platform owner can publish a bounded diagnostic without
    /// changing the non-fatal fallback behavior.
    [[nodiscard]] std::optional<Size> configure(Size physical, float reported_scale) noexcept {
        (void)observe_scale(reported_scale);
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
    void sync_native_geometry_capture_state() noexcept {
        if (!native_geometry_capture_state_) return;
        native_geometry_capture_state_->last_valid_scale_ = last_valid_scale_;
        native_geometry_capture_state_->physical_screen_origin_ = physical_screen_origin_;
    }

    float last_valid_scale_{1.0f};
    bool last_scale_observation_valid_{true};
    Point physical_screen_origin_{};
    bool last_screen_origin_observation_valid_{true};
    Size logical_size_{};
    Size physical_size_{};
    bool renderable_{};
    std::optional<Size> pending_request_;
    std::shared_ptr<ViewNativeGeometryCaptureState> native_geometry_capture_state_;
};

/// Submit one public logical-size request at the native boundary. The native
/// request callback is invoked at most once, and authoritative logical size is
/// deliberately left unchanged until a configure snapshot is received.
template <class NativeRequest>
[[nodiscard]] bool submit_logical_size_request(ViewGeometryState& geometry,
                                               Size logical,
                                               NativeRequest&& native_request) {
    const auto physical = geometry.physical_request(logical);
    if (!physical) return false;
    if (!std::forward<NativeRequest>(native_request)(*physical)) return false;
    geometry.record_successful_request(logical);
    return true;
}

/// Apply one authoritative native configure snapshot and dispatch at most one
/// layout resize for it. Native request submission is intentionally absent
/// from this path so a request echo cannot recurse back into the platform.
template <class Resize>
[[nodiscard]] std::optional<Size> apply_authoritative_configure(ViewGeometryState& geometry,
                                                                 Size physical,
                                                                 float reported_scale,
                                                                 Resize&& resize) {
    auto logical = geometry.configure(physical, reported_scale);
    if (!logical) return std::nullopt;
    std::forward<Resize>(resize)(*logical);
    return logical;
}

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

    [[nodiscard]] bool dispatching() const noexcept { return !dispatch_token_.expired(); }

    template <class Callback>
    bool dispatch_once(Callback&& callback) {
        if (dispatching() || !pending_) return false;

        const Size next = *pending_;
        pending_.reset();
        if (last_notified_ && !differs(next, *last_notified_)) return false;

        // Publish the current value before entering user code. A synchronous
        // queue of the same value is therefore coalesced, while a new value is
        // retained for the next safe checkpoint rather than recursively fired.
        last_notified_ = next;

        // The callback may destroy the owning window/view and therefore this
        // PreferredSizeState. Keep the in-flight marker in separately owned
        // storage so returning from user code never touches destroyed owner
        // memory. Reentrant dispatches observe the weak token while it is live.
        auto dispatch_token = std::make_shared<unsigned char>(0);
        dispatch_token_ = dispatch_token;
        std::forward<Callback>(callback)(next);
        return true;
    }

private:
    [[nodiscard]] static bool differs(Size a, Size b) noexcept {
        return std::fabs(a.w - b.w) > kPreferredSizeEpsilon ||
               std::fabs(a.h - b.h) > kPreferredSizeEpsilon;
    }

    std::weak_ptr<unsigned char> dispatch_token_;
    std::optional<Size> pending_;
    std::optional<Size> last_notified_;
};

} // namespace ui::detail
