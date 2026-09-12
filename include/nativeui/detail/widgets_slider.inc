#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/slider_value.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/slider_style.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

enum class SliderOrientation {
    Horizontal,
    Vertical,
};

enum class SliderVisualState {
    Normal,
    Hover,
    Pressed,
    Focused,
    Disabled,
    ReadOnly,
};

namespace detail {

[[nodiscard]] constexpr SliderVisualState slider_visual_state(
    bool enabled,
    bool read_only,
    bool hovered,
    bool pressed,
    bool focused) noexcept {
    if (!enabled) return SliderVisualState::Disabled;
    if (read_only) return SliderVisualState::ReadOnly;
    if (pressed) return SliderVisualState::Pressed;
    if (hovered) return SliderVisualState::Hover;
    if (focused) return SliderVisualState::Focused;
    return SliderVisualState::Normal;
}

[[nodiscard]] constexpr Color slider_active_color(SliderVisualState state) noexcept {
    switch (state) {
        case SliderVisualState::Hover:
            return colors::caret;
        case SliderVisualState::Pressed:
            return colors::text;
        case SliderVisualState::Disabled:
            return colors::textMuted;
        case SliderVisualState::ReadOnly:
            return colors::track;
        case SliderVisualState::Normal:
        case SliderVisualState::Focused:
            return colors::accent;
    }
    return colors::accent;
}

[[nodiscard]] constexpr Color slider_active_color(
    SliderVisualState state, const ThemePalette& palette) noexcept {
    switch (state) {
        case SliderVisualState::Hover:
            return palette.active_highlight;
        case SliderVisualState::Pressed:
            return palette.text;
        case SliderVisualState::Disabled:
            return palette.disabled;
        case SliderVisualState::ReadOnly:
            return palette.track;
        case SliderVisualState::Normal:
        case SliderVisualState::Focused:
            return palette.accent;
    }
    return palette.accent;
}

// The visual thumb travel is also the pointer-editing domain. Keeping both
// directions in one helper prevents paint and hit/value mapping from drifting
// when track insets or reserved formatter space change.
struct SliderTrackAxis final {
    float start{};
    float end{};
    bool inverted{};

    [[nodiscard]] float position(float fraction) const noexcept {
        if (!std::isfinite(fraction)) fraction = 0.0f;
        const float normalized = std::clamp(fraction, 0.0f, 1.0f);
        const float span = end - start;
        return inverted ? end - span * normalized : start + span * normalized;
    }

    [[nodiscard]] float fraction(float coordinate) const noexcept {
        if (!std::isfinite(coordinate)) return 0.0f;
        const float span = end - start;
        if (!(span > 0.0f)) return 0.0f;
        const float normalized = inverted
            ? (end - coordinate) / span
            : (coordinate - start) / span;
        return std::clamp(normalized, 0.0f, 1.0f);
    }
};

[[nodiscard]] inline SliderTrackAxis slider_track_axis(
    Rect bounds,
    SliderOrientation orientation,
    bool has_formatter = false) noexcept {
    if (orientation == SliderOrientation::Horizontal) {
        const float start = bounds.x + 8.0f;
        const float end = std::max(start, bounds.x + bounds.w - 8.0f);
        return SliderTrackAxis{start, end, false};
    }

    const float track_bottom = has_formatter
        ? bounds.y + bounds.h - 18.0f
        : bounds.y + bounds.h;
    const float start = bounds.y + 8.0f;
    const float end = std::max(start, track_bottom - 8.0f);
    return SliderTrackAxis{start, end, true};
}

class SliderComponent final : public Component, public ThemeBinding {
public:
    using Formatter = std::function<std::string(float)>;

    SliderComponent(
        State<float>& state,
        float minimum,
        float maximum,
        float step,
        SliderOrientation orientation,
        Formatter formatter,
        SliderStyle style)
        : state_(state),
          domain_(minimum, maximum, step),
          orientation_(orientation),
          formatter_(std::move(formatter)),
          style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto& theme = current_theme();
        const auto resolved = resolved_style(focused_);
        const float styled_cross_axis =
            resolved.thumb_diameter + resolved.focus_ring_width * 2.0f;
        const float compact_cross_axis =
            std::max(theme.controls.minimum_hit_target, styled_cross_axis);
        if (orientation_ == SliderOrientation::Horizontal) {
            const float height = formatter_
                ? std::max(theme.controls.control_height + theme.spacing.sm,
                           compact_cross_axis)
                : compact_cross_axis;
            return Size{160.0f, height};
        }
        const float width = formatter_
            ? std::max(theme.controls.control_height + theme.spacing.large * 2.0f,
                       compact_cross_axis)
            : compact_cross_axis;
        return Size{width, 160.0f};
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = state_.observe(
            [invalidate = std::move(invalidate)](const float&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void focus_changed(bool focused, FocusContext& context) override {
        if (focused_ == focused) return;
        const auto before = resolved_style(focused_);
        focused_ = focused;
        invalidate_style_transition(before, resolved_style(focused_), context, true);
    }

    void deactivate(LifecycleContext& context) override {
        if (!dragging_ && !hovered_ && !focused_) return;
        const auto before = resolved_style(focused_);
        const bool focus_changed = focused_;
        dragging_ = false;
        hovered_ = false;
        focused_ = false;
        invalidate_style_transition(before, resolved_style(focused_), context, focus_changed);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (effective_read_only() && mutating_input(event)) {
            const auto before = resolved_style(focused_);
            const bool release_capture = dragging_;
            dragging_ = false;
            if (event.type == InputType::PointerDown || event.type == InputType::PointerMove ||
                event.type == InputType::PointerUp) {
                hovered_ = context.bounds().contains(event.position);
            }
            if (release_capture) context.release_pointer();
            invalidate_style_transition(before, resolved_style(focused_), context);
            return EventResult::Handled;
        }

        switch (event.type) {
            case InputType::PointerDown:
                return begin_drag(event, context);

            case InputType::PointerMove:
                return pointer_move(event, context);

            case InputType::PointerUp:
                if (!dragging_) return EventResult::Ignored;
                return update_drag(event, context, true);

            case InputType::PointerCancel: {
                if (!dragging_) return EventResult::Ignored;
                const auto before = resolved_style(focused_);
                dragging_ = false;
                hovered_ = false;
                invalidate_style_transition(before, resolved_style(focused_), context);
                return EventResult::Handled;
            }

            case InputType::KeyDown:
                return key_down(event, context);

            default:
                return EventResult::Ignored;
        }
    }

    [[nodiscard]] SliderVisualState visual_state(bool focused) const noexcept {
        return slider_visual_state(
            effective_enabled(), effective_read_only(), hovered_, dragging_, focused);
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const float fraction = domain_.fraction(state_.get());
        const auto resolved = resolved_style(context.focused());
        const auto axis = slider_track_axis(
            bounds, orientation_, static_cast<bool>(formatter_));
        const float track_thickness = resolved.track_thickness;
        const float half_track = track_thickness * 0.5f;
        const float thumb_radius = resolved.thumb_diameter * 0.5f;
        const float focus_radius = thumb_radius + resolved.focus_ring_width;

        auto& painter = context.painter();
        if (orientation_ == SliderOrientation::Horizontal) {
            const float track_bottom = formatter_ ? bounds.y + bounds.h - 16.0f : bounds.y + bounds.h;
            const float y = bounds.y + (track_bottom - bounds.y) * 0.5f;
            const float x0 = axis.start;
            const float x1 = axis.end;
            const float thumb_x = axis.position(fraction);
            painter.fill_rounded_rect(
                Rect{x0, y - half_track, x1 - x0, track_thickness}, half_track, resolved.track);
            painter.fill_rounded_rect(
                Rect{x0, y - half_track, thumb_x - x0, track_thickness},
                half_track,
                resolved.active);
            if (context.focused()) {
                painter.circle(Point{thumb_x, y}, focus_radius, resolved.focus_ring);
            }
            painter.circle(Point{thumb_x, y}, thumb_radius, resolved.thumb);
            if (formatter_) {
                const auto text = formatter_(domain_.effective_external(state_.get()));
                painter.text(
                    Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h - 7.0f},
                    text,
                    11.0f,
                    resolved.formatter_text,
                    TextAlign::Center);
            }
        } else {
            const float x = bounds.x + bounds.w * 0.5f;
            const float y0 = axis.start;
            const float y1 = axis.end;
            const float thumb_y = axis.position(fraction);
            painter.fill_rounded_rect(
                Rect{x - half_track, y0, track_thickness, y1 - y0}, half_track, resolved.track);
            painter.fill_rounded_rect(
                Rect{x - half_track, thumb_y, track_thickness, y1 - thumb_y},
                half_track,
                resolved.active);
            if (context.focused()) {
                painter.circle(Point{x, thumb_y}, focus_radius, resolved.focus_ring);
            }
            painter.circle(Point{x, thumb_y}, thumb_radius, resolved.thumb);
            if (formatter_) {
                const auto text = formatter_(domain_.effective_external(state_.get()));
                painter.text(
                    Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h - 7.0f},
                    text,
                    11.0f,
                    resolved.formatter_text,
                    TextAlign::Center);
            }
        }
    }

private:
    template <class Context>
    void invalidate_style_transition(
        const ResolvedSliderStyle& before,
        const ResolvedSliderStyle& after,
        Context& context,
        bool force_paint = false) const {
        switch (classify_slider_style_change(before, after)) {
            case SliderStyleInvalidation::None:
                if (force_paint) context.invalidate();
                return;
            case SliderStyleInvalidation::Paint:
                context.invalidate();
                return;
            case SliderStyleInvalidation::Layout:
                context.invalidate_layout();
                return;
        }
    }

    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = hovered_,
            .pressed = dragging_,
            .focused = focused,
        };
    }

    [[nodiscard]] ResolvedSliderStyle resolved_style(bool focused) const {
        return resolve_slider_style(
            default_slider_style(current_theme()), style_, current_visual_state(focused));
    }

    [[nodiscard]] bool mutating_input(const InputEvent& event) const noexcept {
        if (event.type == InputType::PointerDown) return true;
        if (event.type == InputType::PointerMove || event.type == InputType::PointerUp) {
            return dragging_;
        }
        if (event.type != InputType::KeyDown) return false;
        return event.key == Key::Left || event.key == Key::Right ||
               event.key == Key::Up || event.key == Key::Down ||
               event.key == Key::Home || event.key == Key::End;
    }

    [[nodiscard]] float value_from_pointer(
        const InputEvent& event, const InputContext& context) const noexcept {
        const auto axis = slider_track_axis(
            context.bounds(), orientation_, static_cast<bool>(formatter_));
        const float coordinate = orientation_ == SliderOrientation::Horizontal
            ? event.position.x
            : event.position.y;
        return domain_.value_from_fraction(axis.fraction(coordinate));
    }

    EventResult begin_drag(const InputEvent& event, InputContext& context) {
        const auto before = resolved_style(focused_);
        const float target = value_from_pointer(event, context);
        auto* state = &state_;
        dragging_ = true;
        hovered_ = true;
        context.capture_pointer();
        invalidate_style_transition(before, resolved_style(focused_), context);
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    EventResult pointer_move(const InputEvent& event, InputContext& context) {
        const auto before = resolved_style(focused_);
        const bool inside = context.bounds().contains(event.position);
        hovered_ = inside;
        if (!dragging_) {
            invalidate_style_transition(before, resolved_style(focused_), context);
            return EventResult::Handled;
        }
        return update_drag(event, context, false, before);
    }

    EventResult update_drag(
        const InputEvent& event,
        InputContext& context,
        bool release,
        const ResolvedSliderStyle& before = {}) {
        const auto effective_before = release ? resolved_style(focused_) : before;
        const float target = value_from_pointer(event, context);
        auto* state = &state_;
        if (release) {
            hovered_ = context.bounds().contains(event.position);
            dragging_ = false;
            context.release_pointer();
        }
        invalidate_style_transition(effective_before, resolved_style(focused_), context);
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    EventResult key_down(const InputEvent& event, InputContext&) {
        float target = state_.get();
        const float effective = domain_.effective_external(target);
        const double increment = domain_.keyboard_increment(event.shift);

        switch (event.key) {
            case Key::Left:
            case Key::Down:
                target = domain_.normalize(static_cast<double>(effective) - increment);
                break;
            case Key::Right:
            case Key::Up:
                target = domain_.normalize(static_cast<double>(effective) + increment);
                break;
            case Key::Home:
                target = domain_.normalize(domain_.minimum());
                break;
            case Key::End:
                target = domain_.normalize(domain_.maximum());
                break;
            default:
                return EventResult::Ignored;
        }

        auto* state = &state_;
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    State<float>& state_;
    SliderDomain domain_;
    SliderOrientation orientation_{SliderOrientation::Horizontal};
    Formatter formatter_;
    SliderStyle style_;
    State<float>::Subscription subscription_;
    bool focused_{};
    bool dragging_{};
    bool hovered_{};
};

} // namespace detail

class Slider {
public:
    using Formatter = detail::SliderComponent::Formatter;

    explicit Slider(State<float>& state) : state_(&state) {}

    Slider&& range(float minimum, float maximum) && {
        minimum_ = minimum;
        maximum_ = maximum;
        return std::move(*this);
    }

    Slider&& step(float value) && {
        step_ = value;
        return std::move(*this);
    }

    Slider&& orientation(SliderOrientation value) && {
        orientation_ = value;
        return std::move(*this);
    }

    Slider&& formatter(Formatter value) && {
        formatter_ = std::move(value);
        return std::move(*this);
    }

    Slider&& style(SliderStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const float step = step_;
        const auto orientation = orientation_;
        auto formatter = std::move(formatter_);
        auto style = std::move(style_);
        return Spec{
            [state, minimum, maximum, step, orientation,
             formatter = std::move(formatter), style = std::move(style)]() mutable {
                return std::make_unique<detail::SliderComponent>(
                    *state,
                    minimum,
                    maximum,
                    step,
                    orientation,
                    std::move(formatter),
                    std::move(style));
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{0.0f};
    float maximum_{1.0f};
    float step_{};
    SliderOrientation orientation_{SliderOrientation::Horizontal};
    Formatter formatter_;
    SliderStyle style_;
};

} // namespace ui
