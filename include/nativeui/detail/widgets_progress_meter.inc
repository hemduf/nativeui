#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/progress_style.hpp>
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

class BoundedDisplayComponent final : public Component, public ThemeBinding {
public:
    using Formatter = std::function<std::string(float)>;

    BoundedDisplayComponent(
        State<float>& state,
        float minimum,
        float maximum,
        ProgressOrientation orientation,
        Formatter formatter,
        ProgressBarStyle style)
        : state_(state),
          domain_(minimum, maximum),
          orientation_(orientation),
          formatter_(std::move(formatter)),
          progress_style_(std::move(style)) {}

    BoundedDisplayComponent(
        State<float>& state,
        float minimum,
        float maximum,
        ProgressOrientation orientation,
        Formatter formatter,
        MeterStyle style)
        : state_(state),
          domain_(minimum, maximum),
          orientation_(orientation),
          formatter_(std::move(formatter)),
          meter_style_(std::move(style)),
          meter_(true) {}

    [[nodiscard]] bool focusable() const noexcept override { return false; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style();
        if (orientation_ == ProgressOrientation::Vertical) {
            return formatter_ ? resolved.vertical_formatted_size : resolved.vertical_size;
        }
        return formatter_ ? resolved.horizontal_formatted_size : resolved.horizontal_size;
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
        const auto resolved = resolved_style();
        auto& painter = context.painter();
        const float effective = domain_.effective(state_.get());
        const auto fill = domain_.fill_rect(bounds, effective, orientation_);

        painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.track);
        if (fill.w > 0.0f && fill.h > 0.0f) {
            painter.fill_rounded_rect(fill, resolved.fill_corner_radius, resolved.fill);
        }
        painter.stroke_rounded_rect(
            bounds, resolved.corner_radius, resolved.border_width, resolved.border);

        if (formatter_) {
            const auto text = formatter_(effective);
            painter.text(
                Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
                text,
                resolved.text_size,
                resolved.text,
                TextAlign::Center);
        }
    }

private:
    [[nodiscard]] VisualState current_visual_state() const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
        };
    }

    [[nodiscard]] ResolvedProgressStyle resolved_style() const {
        if (meter_) {
            return resolve_meter_style(
                default_meter_style(current_theme()), meter_style_, current_visual_state());
        }
        return resolve_progress_bar_style(
            default_progress_bar_style(current_theme()),
            progress_style_,
            current_visual_state());
    }

    State<float>& state_;
    BoundedDisplayDomain domain_;
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
    State<float>::Subscription subscription_;
    ProgressBarStyle progress_style_;
    MeterStyle meter_style_;
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

    ProgressBar&& style(ProgressBarStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const auto orientation = orientation_;
        auto formatter = std::move(formatter_);
        auto style = std::move(style_);
        return Spec{
            [state, minimum, maximum, orientation, formatter = std::move(formatter),
             style = std::move(style)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    *state,
                    minimum,
                    maximum,
                    orientation,
                    std::move(formatter),
                    std::move(style));
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{};
    float maximum_{1.0f};
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
    ProgressBarStyle style_;
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

    Meter&& style(MeterStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        const auto orientation = orientation_;
        auto formatter = std::move(formatter_);
        auto style = std::move(style_);
        return Spec{
            [state, minimum, maximum, orientation, formatter = std::move(formatter),
             style = std::move(style)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    *state,
                    minimum,
                    maximum,
                    orientation,
                    std::move(formatter),
                    std::move(style));
            },
            {}};
    }

private:
    State<float>* state_{};
    float minimum_{};
    float maximum_{1.0f};
    ProgressOrientation orientation_{ProgressOrientation::Horizontal};
    Formatter formatter_;
    MeterStyle style_;
};

} // namespace ui
