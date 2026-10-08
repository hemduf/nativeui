#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <nativeui/scroll.hpp>
#include <nativeui/scrollbar_style.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <algorithm>
#include <cmath>

namespace ui {

/// Policy for bringing a descendant into the visible content rectangle.
enum class ScrollAlignment { Nearest, Start, Center, End };

namespace detail {

struct ScrollbarAxisGeometry {
    bool visible{};
    Rect track{};
    Rect thumb{};
};

struct ScrollViewBars {
    ScrollbarAxisGeometry horizontal{};
    ScrollbarAxisGeometry vertical{};
};

enum class ScrollbarAxis { Horizontal, Vertical };

[[nodiscard]] bool scroll_axis_horizontal(ScrollAxis axis) noexcept;

[[nodiscard]] bool scroll_axis_vertical(ScrollAxis axis) noexcept;

[[nodiscard]] float ensure_visible_axis(
    float current,
    float viewport_extent,
    float descendant_start,
    float descendant_extent,
    ScrollAlignment alignment) noexcept;

[[nodiscard]] ScrollbarAxisGeometry make_horizontal_scrollbar(
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    bool vertical_visible,
    float thickness,
    float min_thumb) noexcept;

[[nodiscard]] ScrollbarAxisGeometry make_vertical_scrollbar(
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    bool horizontal_visible,
    float thickness,
    float min_thumb) noexcept;

[[nodiscard]] ScrollViewBars scroll_view_bars_for_metrics(
    ScrollAxis axis,
    Point offset,
    Size metrics_viewport,
    Size content,
    Rect viewport,
    float thickness = 8.0f,
    float min_thumb = 18.0f) noexcept;

[[nodiscard]] ScrollViewBars scroll_view_bars(
    const ScrollState& state,
    Rect viewport,
    float thickness = 8.0f,
    float min_thumb = 18.0f) noexcept;

[[nodiscard]] ScrollbarAxisGeometry scrollbar_for_track(
    const ScrollState& state,
    ScrollbarAxis axis,
    Rect track,
    float min_thumb = 18.0f) noexcept;

class ScrollbarComponent final : public Component, public ThemeBinding {
public:
    ScrollbarComponent(
        ScrollState* state,
        ScrollState::LifetimeToken lifetime,
        ScrollbarAxis axis,
        ScrollbarStyle style);

    ScrollbarComponent(ScrollState& state, ScrollbarAxis axis, ScrollbarStyle style);

    [[nodiscard]] bool pointer_targetable() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;

    EventResult input(const InputEvent& event, InputContext& context) override;

    void deactivate(LifecycleContext& context) override;

    void paint(PaintContext& context) const override;

private:
    [[nodiscard]] bool alive() const noexcept;

    [[nodiscard]] VisualState current_visual_state() const noexcept;

    [[nodiscard]] ResolvedScrollbarStyle resolved_style() const;

    [[nodiscard]] static float sanitized_nonnegative(float value) noexcept;

    [[nodiscard]] static float sanitized_thickness(float value) noexcept;

    template <class Context>
    [[nodiscard]] bool invalidate_style_transition(
        Context& context,
        const ResolvedScrollbarStyle& before) const {
        const auto after = resolved_style();
        if (after == before) return false;
        if (sanitized_thickness(after.thickness) !=
            sanitized_thickness(before.thickness)) {
            context.invalidate_layout();
            return true;
        }
        context.invalidate();
        return false;
    }

    void update_drag(Point position);

    ScrollState* state_{};
    ScrollState::LifetimeToken lifetime_;
    ScrollbarAxis axis_{ScrollbarAxis::Vertical};
    ScrollbarStyle style_;
    bool drag_active_{};
    bool hovered_{};
    float pointer_origin_{};
    float offset_origin_{};
    float travel_{};
    float maximum_{};
};

} // namespace detail

/// Request a logical offset that exposes the specified descendant Rect.
/// May synchronously notify ScrollState observers and propagate exceptions.
[[nodiscard]] bool ensure_visible(
    ScrollState& state,
    Rect descendant_bounds,
    ScrollAlignment alignment = ScrollAlignment::Nearest);

/// Retained wheel/panning input view. Pointer-targetable but excluded
/// from ordinary keyboard Tab focus traversal.
class ScrollViewComponent final : public Component {
public:
    ScrollViewComponent(
        ScrollState* state,
        ScrollAxis axis,
        ScrollState::LifetimeToken lifetime,
        bool pointer_pan);

    ScrollViewComponent(ScrollState& state, bool pointer_pan);

    // Pointer targetability is independent from keyboard focus in the retained
    // tree. ScrollView stays out of Tab traversal while receiving bubbled wheel
    // and optional pan events when descendants do not consume them.
    [[nodiscard]] bool focusable() const noexcept override;
    [[nodiscard]] bool pointer_targetable() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    EventResult input(const InputEvent& event, InputContext& context) override;

    void descendant_focus_changed(Rect descendant_bounds) override;

    void deactivate(LifecycleContext&) override;

    void paint(PaintContext&) const override;

private:
    [[nodiscard]] bool alive() const noexcept;

    [[nodiscard]] static bool same(Point a, Point b) noexcept;

    [[nodiscard]] static float normalized_thickness(float value) noexcept;

    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    bool pointer_pan_{};
    bool pan_active_{};
    Point pan_origin_{};
    Point offset_origin_{};
};

/// Declarative scroll content, scrollbar presentation and optional pointer pan.
/// Owns child/style recipes, but borrows the external ScrollState through
/// its lifetime token. All operations participate in UI/main-thread work.
class ScrollView {
public:
    template <class Child>
/// Borrow the state lifetime token and convert child to an owned Spec.
    ScrollView(ScrollState& state, Child&& child)
        : state_(&state),
          axis_(state.axis()),
          lifetime_(state.lifetime_token()),
          content_(make_spec(std::forward<Child>(child))) {}

/// Opt in/out of pointer-drag panning independently of wheel scrolling.
    ScrollView&& pointer_pan(bool enabled = true) &&;

/// Assign a typed per-instance scrollbar presentation policy.
    ScrollView&& style(ScrollbarStyle value) &&;

/// Consume the content, scrollbar overlay recipes and input policy.
    Spec spec() &&;

private:
    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    Spec content_;
    bool pointer_pan_{};
    ScrollbarStyle style_;
};

} // namespace ui
