#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/slider_value.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace ui {

enum class SliderOrientation {
    Horizontal,
    Vertical,
};

namespace detail {

class SliderComponent final : public Component {
public:
    SliderComponent(
        State<float>& state,
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
            [invalidate = std::move(invalidate)](const float&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void deactivate(LifecycleContext& context) override {
        if (!dragging_) return;
        dragging_ = false;
        context.invalidate();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (effective_read_only() && mutating_input(event)) {
            const bool release_capture = dragging_;
            dragging_ = false;
            if (release_capture) context.release_pointer();
            context.invalidate();
            return EventResult::Handled;
        }

        switch (event.type) {
            case InputType::PointerDown:
                return begin_drag(event, context);

            case InputType::PointerMove:
                if (!dragging_) return EventResult::Ignored;
                return update_drag(event, context, false);

            case InputType::PointerUp:
                if (!dragging_) return EventResult::Ignored;
                return update_drag(event, context, true);

            case InputType::PointerCancel:
                if (!dragging_) return EventResult::Ignored;
                dragging_ = false;
                context.invalidate();
                return EventResult::Handled;

            case InputType::KeyDown:
                return key_down(event, context);

            default:
                return EventResult::Ignored;
        }
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const float fraction = domain_.fraction(state_.get());
        const bool disabled = !effective_enabled();
        const Color track = colors::input;
        const Color active = disabled ? colors::textMuted : colors::accent;
        const Color border = context.focused() ? colors::borderFocus : colors::border;

        auto& painter = context.painter();
        if (orientation_ == SliderOrientation::Horizontal) {
            const float y = bounds.y + bounds.h * 0.5f;
            const float x0 = bounds.x + 8.0f;
            const float x1 = std::max(x0, bounds.x + bounds.w - 8.0f);
            const float thumb_x = x0 + (x1 - x0) * fraction;
            painter.fill_rounded_rect(Rect{x0, y - 2.0f, x1 - x0, 4.0f}, 2.0f, track);
            painter.fill_rounded_rect(Rect{x0, y - 2.0f, thumb_x - x0, 4.0f}, 2.0f, active);
            painter.circle(Point{thumb_x, y}, 7.0f, active);
            if (context.focused()) painter.circle(Point{thumb_x, y}, 9.0f, border);
        } else {
            const float x = bounds.x + bounds.w * 0.5f;
            const float y0 = bounds.y + 8.0f;
            const float y1 = std::max(y0, bounds.y + bounds.h - 8.0f);
            const float thumb_y = y1 - (y1 - y0) * fraction;
            painter.fill_rounded_rect(Rect{x - 2.0f, y0, 4.0f, y1 - y0}, 2.0f, track);
            painter.fill_rounded_rect(Rect{x - 2.0f, thumb_y, 4.0f, y1 - thumb_y}, 2.0f, active);
            painter.circle(Point{x, thumb_y}, 7.0f, active);
            if (context.focused()) painter.circle(Point{x, thumb_y}, 9.0f, border);
        }
    }

private:
    [[nodiscard]] bool mutating_input(const InputEvent& event) const noexcept {
        if (event.type == InputType::PointerDown || event.type == InputType::PointerUp) {
            return true;
        }
        if (event.type == InputType::PointerMove) return dragging_;
        if (event.type != InputType::KeyDown) return false;
        return event.key == Key::Left || event.key == Key::Right ||
               event.key == Key::Up || event.key == Key::Down ||
               event.key == Key::Home || event.key == Key::End;
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

    EventResult begin_drag(const InputEvent& event, InputContext& context) {
        const float target = value_from_pointer(event, context);
        auto* state = &state_;
        dragging_ = true;
        context.capture_pointer();
        context.invalidate();
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    EventResult update_drag(
        const InputEvent& event, InputContext& context, bool release) {
        const float target = value_from_pointer(event, context);
        auto* state = &state_;
        if (release) {
            dragging_ = false;
            context.release_pointer();
        }
        context.invalidate();
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    EventResult key_down(const InputEvent& event, InputContext& context) {
        float target = state_.get();
        const float effective = domain_.effective_external(target);
        const float increment = domain_.keyboard_increment(event.shift);

        switch (event.key) {
            case Key::Left:
            case Key::Down:
                target = domain_.normalize(effective - increment);
                break;
            case Key::Right:
            case Key::Up:
                target = domain_.normalize(effective + increment);
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
        context.invalidate();
        if (state->get() != target) state->set(target);
        return EventResult::Handled;
    }

    State<float>& state_;
    SliderDomain domain_;
    SliderOrientation orientation_{SliderOrientation::Horizontal};
    State<float>::Subscription subscription_;
    bool dragging_{};
};

} // namespace detail

class Slider {
public:
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

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const float step = step_;
        const auto orientation = orientation_;
        return Spec{
            [state, minimum, maximum, step, orientation] {
                return std::make_unique<detail::SliderComponent>(
                    *state, minimum, maximum, step, orientation);
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{0.0f};
    float maximum_{1.0f};
    float step_{};
    SliderOrientation orientation_{SliderOrientation::Horizontal};
};

} // namespace ui
