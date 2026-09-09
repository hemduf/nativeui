#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/slider_value.hpp>
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

class RangeSliderComponent final : public Component {
public:
    RangeSliderComponent(
        State<RangeValue>& state,
        float minimum,
        float maximum,
        float step,
        SliderOrientation orientation)
        : state_(state),
          domain_(minimum, maximum, step),
          orientation_(orientation) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return orientation_ == SliderOrientation::Horizontal
            ? Size{160.0f, 32.0f}
            : Size{32.0f, 160.0f};
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
        const auto state = visual_state(context.focused());
        const Color track = colors::input;
        const Color active = slider_active_color(state);
        const bool focused = state == SliderVisualState::Focused;

        auto& painter = context.painter();
        if (orientation_ == SliderOrientation::Horizontal) {
            const float y = bounds.y + bounds.h * 0.5f;
            const float x0 = bounds.x + 8.0f;
            const float x1 = std::max(x0, bounds.x + bounds.w - 8.0f);
            const float low_x = x0 + (x1 - x0) * low_fraction;
            const float high_x = x0 + (x1 - x0) * high_fraction;
            painter.fill_rounded_rect(Rect{x0, y - 2.0f, x1 - x0, 4.0f}, 2.0f, track);
            painter.fill_rounded_rect(
                Rect{low_x, y - 2.0f, std::max(0.0f, high_x - low_x), 4.0f},
                2.0f,
                active);
            if (focused) {
                painter.circle(Point{low_x, y}, 9.0f, colors::borderFocus);
                painter.circle(Point{high_x, y}, 9.0f, colors::borderFocus);
            }
            painter.circle(Point{low_x, y}, 7.0f, active);
            painter.circle(Point{high_x, y}, 7.0f, active);
        } else {
            const float x = bounds.x + bounds.w * 0.5f;
            const float y0 = bounds.y + 8.0f;
            const float y1 = std::max(y0, bounds.y + bounds.h - 8.0f);
            const float low_y = y1 - (y1 - y0) * low_fraction;
            const float high_y = y1 - (y1 - y0) * high_fraction;
            painter.fill_rounded_rect(Rect{x - 2.0f, y0, 4.0f, y1 - y0}, 2.0f, track);
            painter.fill_rounded_rect(
                Rect{x - 2.0f, high_y, 4.0f, std::max(0.0f, low_y - high_y)},
                2.0f,
                active);
            if (focused) {
                painter.circle(Point{x, low_y}, 9.0f, colors::borderFocus);
                painter.circle(Point{x, high_y}, 9.0f, colors::borderFocus);
            }
            painter.circle(Point{x, low_y}, 7.0f, active);
            painter.circle(Point{x, high_y}, 7.0f, active);
        }
    }

private:
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

    [[nodiscard]] float value_from_pointer(
        const InputEvent& event, const InputContext& context) const noexcept {
        const auto bounds = context.bounds();
        if (orientation_ == SliderOrientation::Horizontal) {
            if (bounds.w <= 0.0f) return domain_.minimum();
            return domain_.value_from_fraction((event.position.x - bounds.x) / bounds.w);
        }
        if (bounds.h <= 0.0f) return domain_.minimum();
        return domain_.value_from_fraction(
            1.0f - (event.position.y - bounds.y) / bounds.h);
    }

    [[nodiscard]] RangeSliderThumb choose_thumb(
        float target, const RangeValue& effective) const noexcept {
        const float lower_distance = std::abs(target - effective.low);
        const float upper_distance = std::abs(target - effective.high);
        if (lower_distance < upper_distance) return RangeSliderThumb::Lower;
        if (upper_distance < lower_distance) return RangeSliderThumb::Upper;
        return has_active_thumb_ ? active_thumb_ : RangeSliderThumb::Lower;
    }

    [[nodiscard]] RangeValue edited_value(float target) const noexcept {
        const auto effective = effective_range(state_.get());
        RangeValue next = effective;
        const float normalized = domain_.normalize(target);
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
        const float target = value_from_pointer(event, context);
        const auto effective = effective_range(state_.get());
        active_thumb_ = choose_thumb(target, effective);
        has_active_thumb_ = true;
        dragging_ = true;
        hovered_ = true;
        context.capture_pointer();
        commit_value(edited_value(target), context);
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
        const float target = value_from_pointer(event, context);
        const auto next = edited_value(target);
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
        const float increment = domain_.keyboard_increment(event.shift);
        float target = current;

        switch (event.key) {
            case Key::Left:
            case Key::Down:
                target = current - increment;
                break;
            case Key::Right:
            case Key::Up:
                target = current + increment;
                break;
            case Key::Home:
                target = active_thumb_ == RangeSliderThumb::Lower
                    ? domain_.minimum()
                    : effective.low;
                break;
            case Key::End:
                target = active_thumb_ == RangeSliderThumb::Lower
                    ? effective.high
                    : domain_.maximum();
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

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const float step = step_;
        const auto orientation = orientation_;
        return Spec{
            [state, minimum, maximum, step, orientation] {
                return std::make_unique<detail::RangeSliderComponent>(
                    *state, minimum, maximum, step, orientation);
            },
            {}};
    }

private:
    State<RangeValue>* state_{};
    float minimum_{0.0f};
    float maximum_{1.0f};
    float step_{};
    SliderOrientation orientation_{SliderOrientation::Horizontal};
};

} // namespace ui
