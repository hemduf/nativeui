#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui {

enum class ProgressOrientation {
    Horizontal,
    Vertical,
};

namespace detail {

class BoundedDisplayDomain final {
public:
    BoundedDisplayDomain(float minimum, float maximum)
        : minimum_(minimum), maximum_(maximum) {
        if (!std::isfinite(minimum_) || !std::isfinite(maximum_) || minimum_ >= maximum_) {
            throw std::invalid_argument(
                "Progress/Meter range must be finite with minimum < maximum");
        }
    }

    [[nodiscard]] float minimum() const noexcept { return minimum_; }
    [[nodiscard]] float maximum() const noexcept { return maximum_; }

    [[nodiscard]] float effective(float value) const noexcept {
        if (!std::isfinite(value)) return minimum_;
        return std::clamp(value, minimum_, maximum_);
    }

    [[nodiscard]] float fraction(float value) const noexcept {
        // The endpoints are finite binary32 values, but maximum-minimum can
        // still overflow binary32 for a valid wide range. Normalize in double
        // so every accepted finite range produces finite [0, 1] geometry.
        const double minimum = static_cast<double>(minimum_);
        const double span = static_cast<double>(maximum_) - minimum;
        const double normalized =
            (static_cast<double>(effective(value)) - minimum) / span;
        return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
    }

    [[nodiscard]] Rect fill_rect(
        Rect bounds,
        float value,
        ProgressOrientation orientation) const noexcept {
        const float normalized = fraction(value);
        if (orientation == ProgressOrientation::Vertical) {
            const float height = bounds.h * normalized;
            return Rect{bounds.x, bounds.y + bounds.h - height, bounds.w, height};
        }
        return Rect{bounds.x, bounds.y, bounds.w * normalized, bounds.h};
    }

private:
    float minimum_{};
    float maximum_{};
};

class BoundedDisplayComponent final : public Component {
public:
    using Formatter = std::function<std::string(float)>;

    BoundedDisplayComponent(
        State<float>& state,
        float minimum,
        float maximum,
        ProgressOrientation orientation,
        Formatter formatter,
        bool meter)
        : state_(state),
          domain_(minimum, maximum),
          orientation_(orientation),
          formatter_(std::move(formatter)),
          meter_(meter) {}

    [[nodiscard]] bool focusable() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        if (orientation_ == ProgressOrientation::Vertical) {
            return Size{formatter_ ? 64.0f : 24.0f, 144.0f};
        }
        return Size{176.0f, formatter_ ? 40.0f : 24.0f};
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = state_.observe(
            [invalidate = std::move(invalidate)](const float&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    EventResult input(const InputEvent&, InputContext&) override {
        return EventResult::Ignored;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        auto& painter = context.painter();
        const float effective = domain_.effective(state_.get());
        const auto fill = domain_.fill_rect(bounds, effective, orientation_);

        painter.fill_rounded_rect(bounds, 6.0f, colors::input);
        if (fill.w > 0.0f && fill.h > 0.0f) {
            painter.fill_rounded_rect(fill, meter_ ? 3.0f : 6.0f, colors::accent);
        }
        painter.stroke_rounded_rect(bounds, 6.0f, 1.0f, colors::border);

        if (formatter_) {
            const auto text = formatter_(effective);
            painter.text(
                Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
                text,
                11.0f,
                colors::text,
                TextAlign::Center);
        }
    }

private:
    State<float>& state_;
    BoundedDisplayDomain domain_;
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
    State<float>::Subscription subscription_;
    bool meter_{};
};

} // namespace detail

class ProgressBar {
public:
    using Formatter = detail::BoundedDisplayComponent::Formatter;

    explicit ProgressBar(State<float>& state, float minimum = 0.0f, float maximum = 1.0f)
        : state_(&state), minimum_(minimum), maximum_(maximum) {}

    ProgressBar&& orientation(ProgressOrientation value) && {
        orientation_ = value;
        return std::move(*this);
    }

    ProgressBar&& formatter(Formatter value) && {
        formatter_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const auto orientation = orientation_;
        auto formatter = std::move(formatter_);
        return Spec{
            [state, minimum, maximum, orientation, formatter = std::move(formatter)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    *state,
                    minimum,
                    maximum,
                    orientation,
                    std::move(formatter),
                    false);
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{};
    float maximum_{1.0f};
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
};

class Meter {
public:
    using Formatter = detail::BoundedDisplayComponent::Formatter;

    explicit Meter(State<float>& state, float minimum = 0.0f, float maximum = 1.0f)
        : state_(&state), minimum_(minimum), maximum_(maximum) {}

    Meter&& orientation(ProgressOrientation value) && {
        orientation_ = value;
        return std::move(*this);
    }

    Meter&& formatter(Formatter value) && {
        formatter_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const auto orientation = orientation_;
        auto formatter = std::move(formatter_);
        return Spec{
            [state, minimum, maximum, orientation, formatter = std::move(formatter)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    *state,
                    minimum,
                    maximum,
                    orientation,
                    std::move(formatter),
                    true);
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{};
    float maximum_{1.0f};
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
};

} // namespace ui
