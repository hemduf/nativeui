#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/slider_value.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/slider_style.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ui {

struct RangeValue {
    float low{};
    float high{1.0f};

    friend bool operator==(const RangeValue&, const RangeValue&) = default;
};

namespace detail {

enum class RangeSliderThumb {
    Lower,
    Upper,
};

class RangeSliderComponent final : public Component, public ThemeBinding {
public:
    RangeSliderComponent(
        State<RangeValue>& state,
        float minimum,
        float maximum,
        float step,
        SliderOrientation orientation,
        SliderStyle style)
        : state_(state),
          domain_(minimum, maximum, step),
          orientation_(orientation),
          style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto& theme = current_theme();
        const auto resolved = resolved_style(false);
        const float styled_cross_axis =
            resolved.thumb_diameter + resolved.focus_ring_width * 2.0f;
        const float compact_cross_axis =
            std::max(theme.controls.minimum_hit_target, styled_cross_axis);
        return orientation_ == SliderOrientation::Horizontal
            ? Size{160.0f, compact_cross_axis}
            : Size{compact_cross_axis, 160.0f};
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = state_.observe(
            [invalidate = std::move(invalidate)](const RangeValue&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void deactivate(LifecycleContext& context) override {
        if (!dragging_ && !hovered_) return;
        dragging_ = false;
        hovered_ = false;
        context.invalidate();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (effective_read_only() && mutating_input(event)) {
            const bool release_capture = dragging_;
            dragging_ = false;
            if (event.type == InputType::PointerDown || event.type == InputType::PointerMove ||
                event.type == InputType::PointerUp) {
                hovered_ = context.bounds().contains(event.position);
            }
            if (release_capture) context.release_pointer();
            context.invalidate();
            return EventResult::Handled;
        }

        switch (event.type) {
            case InputType::PointerDown:
                return begin_drag(event, context);

            case InputType::PointerMove:
                return pointer_move(event, context);

            case InputType::PointerUp:
                if (!dragging_) return EventResult::Ignored;
                hovered_ = context.bounds().contains(event.position);
                return update_drag(event, context, true);

            case InputType::PointerCancel:
                if (!dragging_) return EventResult::Ignored;
                dragging_ = false;
                hovered_ = false;
                context.invalidate();
                return EventResult::Handled;

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
        const auto effective = effective_range(state_.get());
        const float low_fraction = domain_.fraction(effective.low);
        const float high_fraction = domain_.fraction(effective.high);
        const auto resolved = resolved_style(context.focused());
        const auto axis = slider_track_axis(bounds, orientation_);
        const float track_thickness = resolved.track_thickness;
        const float half_track = track_thickness * 0.5f;
        const float thumb_radius = resolved.thumb_diameter * 0.5f;
        const float focus_radius = thumb_radius + resolved.focus_ring_width;

        auto& painter = context.painter();
        if (orientation_ == SliderOrientation::Horizontal) {
            const float y = bounds.y + bounds.h * 0.5f;
            const float x0 = axis.start;
            const float x1 = axis.end;
            const float low_x = axis.position(low_fraction);
            const float high_x = axis.position(high_fraction);
            painter.fill_rounded_rect(
                Rect{x0, y - half_track, x1 - x0, track_thickness},
                half_track,
                resolved.track);
            painter.fill_rounded_rect(
                Rect{low_x, y - half_track,
                     std::max(0.0f, high_x - low_x), track_thickness},
                half_track,
                resolved.active);
            if (context.focused()) {
                painter.circle(Point{low_x, y}, focus_radius, resolved.focus_ring);
                painter.circle(Point{high_x, y}, focus_radius, resolved.focus_ring);
            }
            painter.circle(Point{low_x, y}, thumb_radius, resolved.thumb);
            painter.circle(Point{high_x, y}, thumb_radius, resolved.thumb);
        } else {
            const float x = bounds.x + bounds.w * 0.5f;
            const float y0 = axis.start;
            const float y1 = axis.end;
            const float low_y = axis.position(low_fraction);
            const float high_y = axis.position(high_fraction);
            painter.fill_rounded_rect(
                Rect{x - half_track, y0, track_thickness, y1 - y0},
                half_track,
                resolved.track);
            painter.fill_rounded_rect(
                Rect{x - half_track, high_y, track_thickness,
                     std::max(0.0f, low_y - high_y)},
                half_track,
                resolved.active);
            if (context.focused()) {
                painter.circle(Point{x, low_y}, focus_radius, resolved.focus_ring);
                painter.circle(Point{x, high_y}, focus_radius, resolved.focus_ring);
            }
            painter.circle(Point{x, low_y}, thumb_radius, resolved.thumb);
            painter.circle(Point{x, high_y}, thumb_radius, resolved.thumb);
        }
    }

private:
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

    [[nodiscard]] RangeValue effective_range(const RangeValue& value) const noexcept {
        RangeValue effective{
            std::isfinite(value.low)
                ? std::clamp(value.low, domain_.minimum(), domain_.maximum())
                : domain_.minimum(),
            std::isfinite(value.high)
                ? std::clamp(value.high, domain_.minimum(), domain_.maximum())
                : domain_.maximum(),
        };
        if (effective.low > effective.high) std::swap(effective.low, effective.high);
        return effective;
    }

    [[nodiscard]] float raw_value_from_pointer(
        const InputEvent& event, const InputContext& context) const noexcept {
        const auto axis = slider_track_axis(context.bounds(), orientation_);
        const float coordinate = orientation_ == SliderOrientation::Horizontal
            ? event.position.x
            : event.position.y;
        return domain_.raw_value_from_fraction(axis.fraction(coordinate));
    }

    [[nodiscard]] RangeSliderThumb choose_thumb(
        float raw_target, const RangeValue& effective) const noexcept {
        const double lower_distance = std::abs(
            static_cast<double>(raw_target) - static_cast<double>(effective.low));
        const double upper_distance = std::abs(
            static_cast<double>(raw_target) - static_cast<double>(effective.high));
        if (lower_distance < upper_distance) return RangeSliderThumb::Lower;
        if (upper_distance < lower_distance) return RangeSliderThumb::Upper;
        return has_active_thumb_ ? active_thumb_ : RangeSliderThumb::Lower;
    }

    [[nodiscard]] RangeValue edited_value(double raw_target) const noexcept {
        const auto effective = effective_range(state_.get());
        RangeValue next = effective;
        const float normalized = domain_.normalize(raw_target);
        if (active_thumb_ == RangeSliderThumb::Lower) {
            next.low = std::min(normalized, effective.high);
        } else {
            next.high = std::max(normalized, effective.low);
        }
        return next;
    }

    void commit_value(const RangeValue& value, InputContext& context) {
        auto* state = &state_;
        context.invalidate();
        if (!(state->get() == value)) state->set(value);
    }

    EventResult begin_drag(const InputEvent& event, InputContext& context) {
        const float raw_target = raw_value_from_pointer(event, context);
        const auto effective = effective_range(state_.get());
        active_thumb_ = choose_thumb(raw_target, effective);
        has_active_thumb_ = true;
        dragging_ = true;
        hovered_ = true;
        context.capture_pointer();
        commit_value(edited_value(raw_target), context);
        return EventResult::Handled;
    }

    EventResult pointer_move(const InputEvent& event, InputContext& context) {
        const bool inside = context.bounds().contains(event.position);
        if (!dragging_) {
            if (hovered_ != inside) {
                hovered_ = inside;
                context.invalidate();
            }
            return EventResult::Handled;
        }
        hovered_ = inside;
        return update_drag(event, context, false);
    }

    EventResult update_drag(
        const InputEvent& event, InputContext& context, bool release) {
        const float raw_target = raw_value_from_pointer(event, context);
        const auto next = edited_value(raw_target);
        if (release) {
            dragging_ = false;
            context.release_pointer();
        }
        commit_value(next, context);
        return EventResult::Handled;
    }

    EventResult key_down(const InputEvent& event, InputContext& context) {
        const bool value_key = event.key == Key::Left || event.key == Key::Right ||
                               event.key == Key::Up || event.key == Key::Down ||
                               event.key == Key::Home || event.key == Key::End;
        if (!value_key) return EventResult::Ignored;

        if (!has_active_thumb_) {
            active_thumb_ = RangeSliderThumb::Lower;
            has_active_thumb_ = true;
        }

        const auto effective = effective_range(state_.get());
        const float current = active_thumb_ == RangeSliderThumb::Lower
            ? effective.low
            : effective.high;
        const double increment = domain_.keyboard_increment(event.shift);
        double target = static_cast<double>(current);

        switch (event.key) {
            case Key::Left:
            case Key::Down:
                target = static_cast<double>(current) - increment;
                break;
            case Key::Right:
            case Key::Up:
                target = static_cast<double>(current) + increment;
                break;
            case Key::Home:
                target = active_thumb_ == RangeSliderThumb::Lower
                    ? static_cast<double>(domain_.minimum())
                    : static_cast<double>(effective.low);
                break;
            case Key::End:
                target = active_thumb_ == RangeSliderThumb::Lower
                    ? static_cast<double>(effective.high)
                    : static_cast<double>(domain_.maximum());
                break;
            default:
                return EventResult::Ignored;
        }

        commit_value(edited_value(target), context);
        return EventResult::Handled;
    }

    State<RangeValue>& state_;
    SliderDomain domain_;
    SliderOrientation orientation_{SliderOrientation::Horizontal};
    SliderStyle style_;
    State<RangeValue>::Subscription subscription_;
    RangeSliderThumb active_thumb_{RangeSliderThumb::Lower};
    bool has_active_thumb_{};
    bool dragging_{};
    bool hovered_{};
};

} // namespace detail

class RangeSlider {
public:
    explicit RangeSlider(State<RangeValue>& state) : state_(&state) {}

    RangeSlider&& range(float minimum, float maximum) && {
        minimum_ = minimum;
        maximum_ = maximum;
        return std::move(*this);
    }

    RangeSlider&& step(float value) && {
        step_ = value;
        return std::move(*this);
    }

    RangeSlider&& orientation(SliderOrientation value) && {
        orientation_ = value;
        return std::move(*this);
    }

    RangeSlider&& style(SliderStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const float step = step_;
        const auto orientation = orientation_;
        auto style = std::move(style_);
        return Spec{
            [state, minimum, maximum, step, orientation, style = std::move(style)]() mutable {
                return std::make_unique<detail::RangeSliderComponent>(
                    *state, minimum, maximum, step, orientation, std::move(style));
            },
            {}};
    }

private:
    State<RangeValue>* state_{};
    float minimum_{0.0f};
    float maximum_{1.0f};
    float step_{};
    SliderOrientation orientation_{SliderOrientation::Horizontal};
    SliderStyle style_;
};

} // namespace ui
