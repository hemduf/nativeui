#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
struct OverlayLifetimeToken final {};
struct OverlayState;
} // namespace detail

class OverlayHandle {
public:
    OverlayHandle() = default;

    [[nodiscard]] bool valid() const noexcept {
        return id_ != 0 && !owner_.expired() && !lifetime_.expired();
    }

    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] bool operator==(const OverlayHandle& other) const noexcept {
        return id_ == other.id_ && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }

private:
    friend struct detail::OverlayState;

    OverlayHandle(std::weak_ptr<const detail::OverlayOwnerToken> owner,
                  std::weak_ptr<const detail::OverlayLifetimeToken> lifetime,
                  std::uint64_t id) noexcept
        : owner_(std::move(owner)), lifetime_(std::move(lifetime)), id_(id) {}

    std::weak_ptr<const detail::OverlayOwnerToken> owner_;
    std::weak_ptr<const detail::OverlayLifetimeToken> lifetime_;
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

/// Read-only diagnostic view of one T061 overlay entry. It exposes only the
/// generic policy/placement metadata needed to verify retained overlay
/// behavior; content components, platform objects and overlay state ownership
/// never leave the owning UI.
struct OverlayEntryInfo {
    std::uint64_t id{};
    OverlayMode mode{OverlayMode::NonModal};
    OverlayPointerPolicy pointer_policy{OverlayPointerPolicy::Normal};
    std::optional<NodeId> anchor;
    OverlayPlacement placement{OverlayPlacement::Auto};
    bool resolved{};
    Rect bounds{};
};

namespace detail {

[[nodiscard]] inline float overlay_finite_extent(float value) noexcept {
    return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}

[[nodiscard]] inline float overlay_finite_coordinate(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] inline Rect overlay_sanitize_viewport(Rect viewport) noexcept {
    viewport.x = overlay_finite_coordinate(viewport.x, 0.0f);
    viewport.y = overlay_finite_coordinate(viewport.y, 0.0f);
    viewport.w = overlay_finite_extent(viewport.w);
    viewport.h = overlay_finite_extent(viewport.h);
    return viewport;
}

[[nodiscard]] inline Rect overlay_sanitize_anchor(Rect anchor, Rect viewport) noexcept {
    anchor.x = overlay_finite_coordinate(anchor.x, viewport.x);
    anchor.y = overlay_finite_coordinate(anchor.y, viewport.y);
    anchor.w = overlay_finite_extent(anchor.w);
    anchor.h = overlay_finite_extent(anchor.h);
    return anchor;
}

[[nodiscard]] inline Rect overlay_clamp_origin(Rect viewport, Rect candidate) noexcept {
    candidate.w = overlay_finite_extent(candidate.w);
    candidate.h = overlay_finite_extent(candidate.h);
    candidate.x = overlay_finite_coordinate(candidate.x, viewport.x);
    candidate.y = overlay_finite_coordinate(candidate.y, viewport.y);

    if (candidate.w <= viewport.w) {
        const float max_x = viewport.x + viewport.w - candidate.w;
        if (candidate.x < viewport.x) candidate.x = viewport.x;
        else if (candidate.x > max_x) candidate.x = max_x;
    } else {
        candidate.x = viewport.x;
    }

    if (candidate.h <= viewport.h) {
        const float max_y = viewport.y + viewport.h - candidate.h;
        if (candidate.y < viewport.y) candidate.y = viewport.y;
        else if (candidate.y > max_y) candidate.y = max_y;
    } else {
        candidate.y = viewport.y;
    }
    return candidate;
}

[[nodiscard]] inline bool overlay_fully_fits(Rect viewport, Rect candidate) noexcept {
    return candidate.x >= viewport.x && candidate.y >= viewport.y &&
           candidate.x + candidate.w <= viewport.x + viewport.w &&
           candidate.y + candidate.h <= viewport.y + viewport.h;
}

[[nodiscard]] inline float overlay_intersection_area(Rect viewport, Rect candidate) noexcept {
    const Rect clipped = intersect(viewport, candidate);
    return clipped.w * clipped.h;
}

[[nodiscard]] inline Rect overlay_side_candidate(
    Rect anchor, Size content, OverlayPlacement placement) noexcept {
    switch (placement) {
        case OverlayPlacement::AnchorBelow:
            return {anchor.x, anchor.y + anchor.h, content.w, content.h};
        case OverlayPlacement::AnchorAbove:
            return {anchor.x, anchor.y - content.h, content.w, content.h};
        case OverlayPlacement::AnchorRight:
            return {anchor.x + anchor.w, anchor.y, content.w, content.h};
        case OverlayPlacement::AnchorLeft:
            return {anchor.x - content.w, anchor.y, content.w, content.h};
        case OverlayPlacement::Center:
        case OverlayPlacement::Auto:
            break;
    }
    return {anchor.x, anchor.y, content.w, content.h};
}

[[nodiscard]] inline OverlayPlacement overlay_opposite_side(OverlayPlacement placement) noexcept {
    switch (placement) {
        case OverlayPlacement::AnchorBelow: return OverlayPlacement::AnchorAbove;
        case OverlayPlacement::AnchorAbove: return OverlayPlacement::AnchorBelow;
        case OverlayPlacement::AnchorRight: return OverlayPlacement::AnchorLeft;
        case OverlayPlacement::AnchorLeft: return OverlayPlacement::AnchorRight;
        case OverlayPlacement::Center:
        case OverlayPlacement::Auto:
            return placement;
    }
    return placement;
}

/// Deterministic T061 placement policy. Content retains its measured natural
/// size; only the final origin is clamped. For a requested side, the opposite
/// side is preferred only when the requested side does not fit, or when neither
/// fits and the opposite has strictly greater viewport intersection. Equal-area
/// ties therefore preserve the requested side. Auto uses Below/Above/Right/Left
/// in that exact priority for both full-fit and equal-area selection.
[[nodiscard]] inline Rect overlay_placement_bounds(
    Rect viewport, Rect anchor, Size natural_size, OverlayPlacement placement) noexcept {
    viewport = overlay_sanitize_viewport(viewport);
    anchor = overlay_sanitize_anchor(anchor, viewport);
    const Size content{
        overlay_finite_extent(natural_size.w),
        overlay_finite_extent(natural_size.h)};

    if (placement == OverlayPlacement::Center) {
        return overlay_clamp_origin(
            viewport,
            Rect{
                viewport.x + (viewport.w - content.w) * 0.5f,
                viewport.y + (viewport.h - content.h) * 0.5f,
                content.w,
                content.h});
    }

    if (placement == OverlayPlacement::Auto) {
        constexpr OverlayPlacement priority[] = {
            OverlayPlacement::AnchorBelow,
            OverlayPlacement::AnchorAbove,
            OverlayPlacement::AnchorRight,
            OverlayPlacement::AnchorLeft,
        };

        Rect best = overlay_side_candidate(anchor, content, priority[0]);
        float best_area = overlay_intersection_area(viewport, best);
        for (const auto side : priority) {
            const Rect candidate = overlay_side_candidate(anchor, content, side);
            if (overlay_fully_fits(viewport, candidate)) {
                return overlay_clamp_origin(viewport, candidate);
            }
            const float area = overlay_intersection_area(viewport, candidate);
            if (area > best_area) {
                best = candidate;
                best_area = area;
            }
        }
        return overlay_clamp_origin(viewport, best);
    }

    const Rect requested = overlay_side_candidate(anchor, content, placement);
    if (overlay_fully_fits(viewport, requested)) {
        return overlay_clamp_origin(viewport, requested);
    }

    const Rect opposite = overlay_side_candidate(
        anchor, content, overlay_opposite_side(placement));
    if (overlay_fully_fits(viewport, opposite)) {
        return overlay_clamp_origin(viewport, opposite);
    }

    const float requested_area = overlay_intersection_area(viewport, requested);
    const float opposite_area = overlay_intersection_area(viewport, opposite);
    return overlay_clamp_origin(
        viewport, opposite_area > requested_area ? opposite : requested);
}

struct OverlayEntry {
    std::uint64_t id{};
    OverlaySpec spec;
    std::shared_ptr<const OverlayLifetimeToken> lifetime;
    std::optional<Rect> anchor_bounds;
    Rect resolved_bounds{};
    bool resolved{};
};

// One OverlayState belongs to one UI. It stores only logical overlay state and
// one T058 structural invalidator; no overlay registry or mutation queue is
// shared process-wide between simultaneous NativeUI/plugin instances.
struct OverlayState {
    std::shared_ptr<const OverlayOwnerToken> owner{
        std::make_shared<const OverlayOwnerToken>()};
    std::uint64_t next_id{1};
    std::vector<OverlayEntry> entries;
    std::function<void()> structural_invalidator;

    void invalidate_structure() const {
        if (structural_invalidator) structural_invalidator();
    }

    [[nodiscard]] OverlayHandle show(OverlaySpec overlay) {
        if (overlay.mode == OverlayMode::Modal &&
            overlay.pointer_policy == OverlayPointerPolicy::Ignore) {
            return {};
        }
        if (next_id == 0) return {};

        const auto id = next_id;
        if (next_id == std::numeric_limits<std::uint64_t>::max()) next_id = 0;
        else ++next_id;

        auto lifetime = std::make_shared<const OverlayLifetimeToken>();
        entries.push_back(OverlayEntry{id, std::move(overlay), lifetime, std::nullopt, {}});
        invalidate_structure();
        return OverlayHandle{owner, lifetime, id};
    }

    bool close(OverlayHandle handle) {
        const auto handle_owner = handle.owner_.lock();
        const auto lifetime = handle.lifetime_.lock();
        if (!handle_owner || handle_owner != owner || !lifetime || handle.id_ == 0) {
            return false;
        }

        const auto it = std::find_if(entries.begin(), entries.end(), [&](const OverlayEntry& entry) {
            return entry.id == handle.id_ && entry.lifetime == lifetime;
        });
        if (it == entries.end()) return false;
        entries.erase(it);
        invalidate_structure();
        return true;
    }

    bool close_id(std::uint64_t id) {
        const auto it = std::find_if(entries.begin(), entries.end(), [id](const OverlayEntry& entry) {
            return entry.id == id;
        });
        if (it == entries.end()) return false;
        entries.erase(it);
        invalidate_structure();
        return true;
    }
};

class OverlayEntryComponent final : public Component, public PointerDescendantPolicy {
public:
    OverlayEntryComponent(OverlayMode mode, OverlayPointerPolicy pointer_policy)
        : mode_(mode), pointer_policy_(pointer_policy) {}

    [[nodiscard]] bool focusable() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] bool pointer_targetable() const noexcept override {
        return pointer_policy_ == OverlayPointerPolicy::Normal;
    }

    [[nodiscard]] bool pointer_descendants_targetable() const noexcept override {
        return pointer_policy_ == OverlayPointerPolicy::Normal;
    }

    [[nodiscard]] bool is_focus_scope() const noexcept override {
        // Modal entries are active trapping scopes. Pointer-transparent entries
        // are deliberately inactive focus boundaries so their focusable
        // descendants do not enter ordinary Tab traversal merely because a
        // tooltip/visual overlay was shown.
        return mode_ == OverlayMode::Modal ||
               pointer_policy_ == OverlayPointerPolicy::Ignore;
    }

    [[nodiscard]] bool focus_scope_active() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] bool focus_scope_traps() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override {
        return 1;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        const bool pointer_event = event.type == InputType::PointerDown ||
                                   event.type == InputType::PointerMove ||
                                   event.type == InputType::PointerUp ||
                                   event.type == InputType::PointerCancel ||
                                   event.type == InputType::PointerWheel;
        if (pointer_event && pointer_policy_ == OverlayPointerPolicy::Normal) {
            return EventResult::Handled;
        }

        const bool keyboard_event = event.type == InputType::KeyDown ||
                                    event.type == InputType::KeyUp ||
                                    event.type == InputType::Command ||
                                    event.type == InputType::TextInput ||
                                    event.type == InputType::Composition;
        if (keyboard_event && mode_ == OverlayMode::Modal) return EventResult::Handled;
        return EventResult::Ignored;
    }

    void paint(PaintContext&) const override {}

private:
    OverlayMode mode_{};
    OverlayPointerPolicy pointer_policy_{};
};

/// Invisible full-viewport sibling placed immediately below one modal overlay.
/// Reverse retained hit testing means overlays created above that modal remain
/// interactive, the modal content itself wins inside its bounds, and this node
/// absorbs pointer input everywhere else before any lower overlay/root content.
/// It owns no second routing queue or global state.
class OverlayModalBarrierComponent final : public Component {
public:
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return {};
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        const bool pointer_event = event.type == InputType::PointerDown ||
                                   event.type == InputType::PointerMove ||
                                   event.type == InputType::PointerUp ||
                                   event.type == InputType::PointerCancel ||
                                   event.type == InputType::PointerWheel;
        return pointer_event ? EventResult::Handled : EventResult::Ignored;
    }

    void paint(PaintContext&) const override {}
};

class OverlayHostComponent final : public Component, public DynamicChildrenSource {
public:
    explicit OverlayHostComponent(std::shared_ptr<OverlayState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints,
        std::size_t child_index,
        std::size_t) const override {
        return child_index == 0 ? constraints : constraints.loosen();
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override {
        if (placements.empty()) return;
        placements.front().bounds = bounds;

        std::size_t child_index = 1;
        for (auto& entry : state_->entries) {
            if (entry.spec.mode == OverlayMode::Modal) {
                if (child_index >= placements.size()) break;
                placements[child_index].bounds = bounds;
                ++child_index;
            }
            if (child_index >= placements.size() || child_index >= children.size()) break;

            const auto size = children[child_index].preferred;
            const bool has_anchor = entry.spec.anchor.has_value() && entry.anchor_bounds.has_value();
            const Rect anchor = has_anchor ? *entry.anchor_bounds : bounds;
            const OverlayPlacement placement = has_anchor
                ? entry.spec.placement
                : OverlayPlacement::Center;
            entry.resolved_bounds = overlay_placement_bounds(bounds, anchor, size, placement);
            entry.resolved = true;
            placements[child_index].bounds = entry.resolved_bounds;
            ++child_index;
        }
    }

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        std::vector<std::string> keys;
        keys.reserve(state_->entries.size() * 2 + 1);
        keys.emplace_back("root");
        for (const auto& entry : state_->entries) {
            if (entry.spec.mode == OverlayMode::Modal) {
                keys.push_back("modal-barrier:" + std::to_string(entry.id));
            }
            keys.push_back("overlay:" + std::to_string(entry.id));
        }
        return keys;
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        std::vector<DynamicChildSpec> children;
        children.reserve(state_->entries.size() * 2 + 1);

        // `root` is present as the host's statically compiled child from the
        // first mount and its key is permanently retained. T058 never consults
        // the Spec for a retained key, so keep only a zero-allocation sentinel
        // here instead of owning/copying the complete application Spec tree.
        children.push_back(DynamicChildSpec{"root", Spec{}});
        for (const auto& entry : state_->entries) {
            if (entry.spec.mode == OverlayMode::Modal) {
                children.push_back(DynamicChildSpec{
                    "modal-barrier:" + std::to_string(entry.id),
                    Spec{[] { return std::make_unique<OverlayModalBarrierComponent>(); }, {}}});
            }

            Spec wrapper{
                [mode = entry.spec.mode, pointer_policy = entry.spec.pointer_policy] {
                    return std::make_unique<OverlayEntryComponent>(mode, pointer_policy);
                },
                {entry.spec.content}};
            children.push_back(DynamicChildSpec{
                "overlay:" + std::to_string(entry.id), std::move(wrapper)});
        }
        return children;
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        state_->structural_invalidator = std::move(invalidator);
    }

    void paint(PaintContext&) const override {}

private:
    std::shared_ptr<OverlayState> state_;
};

[[nodiscard]] inline Spec make_overlay_host_spec(
    Spec root, const std::shared_ptr<OverlayState>& state) {
    std::vector<Spec> children;
    children.reserve(1);
    children.push_back(std::move(root));
    return Spec{
        [state] { return std::make_unique<OverlayHostComponent>(state); },
        std::move(children)};
}

} // namespace detail

} // namespace ui
