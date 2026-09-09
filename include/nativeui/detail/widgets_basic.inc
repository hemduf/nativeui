#pragma once

#include <nativeui/component.hpp>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

inline void paint_text_in_rect(Painter& painter, Rect bounds, std::string_view text, const TextStyle& style) {
    Point anchor{};
    anchor.y = bounds.y + bounds.h * 0.5f;
    switch (style.align) {
        case TextAlign::Left: anchor.x = bounds.x; break;
        case TextAlign::Center: anchor.x = bounds.x + bounds.w * 0.5f; break;
        case TextAlign::Right: anchor.x = bounds.x + bounds.w; break;
    }
    painter.text(anchor, text, style);
}

} // namespace detail

class LabelComponent final : public Component {
public:
    LabelComponent(std::string text, TextStyle style)
        : text_(std::move(text)), style_(style) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto metrics = TextService::measure(text_, style_);
        return Size{metrics.width, metrics.height};
    }

    void paint(PaintContext& context) const override {
        detail::paint_text_in_rect(context.painter(), context.bounds(), text_, style_);
    }

private:
    std::string text_;
    TextStyle style_{};
};

class HeaderComponent final : public Component {
public:
    explicit HeaderComponent(std::string title) : title_(std::move(title)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return Size{640.0f, 70.0f};
    }

    void paint(PaintContext& p) const override {
        const auto b = p.bounds();
        auto& dl = p.painter();

        TextStyle title_style{};
        title_style.size = 21.0f;
        title_style.color = colors::text;

        TextStyle subtitle_style{};
        subtitle_style.size = 11.0f;
        subtitle_style.color = colors::textMuted;

        detail::paint_text_in_rect(
            dl, Rect{b.x, b.y + 10.0f, b.w, 26.0f}, title_, title_style);
        detail::paint_text_in_rect(
            dl, Rect{b.x, b.y + 43.0f, b.w, 12.0f}, "SATURATION / CHARACTER", subtitle_style);
        dl.line(
            Point{b.x, b.y + b.h - 1.0f},
            Point{b.x + b.w, b.y + b.h - 1.0f},
            1.0f,
            colors::border);
    }

private:
    std::string title_;
};


class CanvasComponent final : public Component {
public:
    using DrawCallback = std::function<void(CanvasContext2D&)>;
    using InputCallback = std::function<EventResult(const InputEvent&, CanvasInputContext&)>;

    CanvasComponent(Size size, DrawCallback draw, InputCallback input, bool focusable)
        : size_(size), draw_(std::move(draw)), input_(std::move(input)), focusable_(focusable) {
        size_.w = std::max(1.0f, size_.w);
        size_.h = std::max(1.0f, size_.h);
    }

    [[nodiscard]] bool focusable() const noexcept override { return focusable_; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return size_;
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (!input_) return EventResult::Ignored;

        InputEvent local = event;
        if (event.type == InputType::PointerDown || event.type == InputType::PointerMove ||
            event.type == InputType::PointerUp || event.type == InputType::PointerWheel ||
            event.type == InputType::DropOffer || event.type == InputType::DropData) {
            const auto bounds = context.bounds();
            local.position.x -= bounds.x;
            local.position.y -= bounds.y;
        }

        CanvasInputContext canvas_context{context};
        return input_(local, canvas_context);
    }

    void paint(PaintContext& context) const override {
        if (!draw_) return;

        auto& painter = context.painter();
        painter.push_clip(context.bounds());
        {
            // Canvas callbacks always operate in local component coordinates.
            // Protect this base translation so user restore() calls cannot cross
            // the local coordinate frame established by the widget.
            auto local_scope = painter.scoped_state();
            painter.translate(context.bounds().x, context.bounds().y);
            CanvasContext2D canvas_context{
                painter, Rect{0.0f, 0.0f, context.bounds().w, context.bounds().h},
                context.focused()};
            draw_(canvas_context);
        }
        painter.pop_clip();
    }

private:
    Size size_{};
    DrawCallback draw_;
    InputCallback input_;
    bool focusable_{};
};


class KnobComponent final : public Component {
public:
    KnobComponent(std::string label, State<float>& state, float minimum, float maximum)
        : label_(std::move(label)), state_(state), value_(state.get()), minimum_(minimum), maximum_(maximum) {
        if (maximum_ <= minimum_) maximum_ = minimum_ + 1.0f;
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return Size{176.0f, 182.0f};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        subscription_ = state_.observe([this, invalidate = std::move(invalidate)](const float& value) {
            value_ = std::clamp(value, minimum_, maximum_);
            invalidate();
        });
    }

    EventResult input(const InputEvent& event, InputContext& ctx) override {
        const float range = maximum_ - minimum_;
        if (event.type == InputType::KeyDown) {
            float delta = 0.0f;
            const float step = event.shift ? range * 0.002f : range * 0.01f;
            if (event.key == Key::Left || event.key == Key::Down) delta = -step;
            if (event.key == Key::Right || event.key == Key::Up) delta = +step;
            if (delta == 0.0f) return EventResult::Ignored;

            if (!effective_read_only()) {
                state_.set(std::clamp(value_ + delta, minimum_, maximum_));
                ctx.invalidate();
            }
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerDown) {
            if (effective_read_only()) return EventResult::Handled;
            drag_gesture_.begin(event.position);
            drag_start_value_ = value_;
            ctx.capture_pointer();
            ctx.invalidate();
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerMove && drag_gesture_.active()) {
            if (effective_read_only()) {
                (void)drag_gesture_.cancel();
                ctx.release_pointer();
                ctx.invalidate();
                return EventResult::Handled;
            }
            const auto drag = drag_gesture_.move(event.position);
            if (drag.dragging) {
                const float value_delta = drag_value_delta(
                    drag.total, DragAxis::Vertical, range / 180.0f, true);
                state_.set(std::clamp(drag_start_value_ + value_delta, minimum_, maximum_));
            }
            ctx.invalidate();
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerUp && drag_gesture_.active()) {
            if (effective_read_only()) {
                (void)drag_gesture_.cancel();
            } else {
                (void)drag_gesture_.end(event.position);
            }
            ctx.release_pointer();
            ctx.invalidate();
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerCancel && drag_gesture_.active()) {
            (void)drag_gesture_.cancel();
            ctx.invalidate();
            return EventResult::Handled;
        }

        return EventResult::Ignored;
    }

    void paint(PaintContext& p) const override {
        const auto b = p.bounds();
        auto& dl = p.painter();
        dl.fill_rounded_rect(b, 14.0f, colors::panel);
        dl.stroke_rounded_rect(b, 14.0f, p.focused() ? 2.0f : 1.0f,
                               p.focused() ? colors::borderFocus : colors::border);
        dl.text(Point{b.x + b.w * 0.5f, b.y + 24.0f}, label_, 13.0f, colors::text, TextAlign::Center);

        const Point center{b.x + b.w * 0.5f, b.y + 93.0f};
        constexpr float radius = 42.0f;
        constexpr float arc_radius = 51.0f;
        constexpr float start = 0.75f * kPi;
        constexpr float sweep = 1.5f * kPi;
        const float normalized = std::clamp((value_ - minimum_) / (maximum_ - minimum_), 0.0f, 1.0f);
        const float angle = start + normalized * sweep;

        dl.arc(center, arc_radius, start, start + sweep, 4.0f, colors::track);
        if (normalized > 0.001f) dl.arc(center, arc_radius, start, angle, 4.0f, colors::accent);
        dl.circle(center, radius, colors::knob);
        dl.circle(center, radius - 7.0f, colors::knobInner);
        const Point marker{center.x + std::cos(angle) * (radius - 13.0f),
                           center.y + std::sin(angle) * (radius - 13.0f)};
        dl.line(center, marker, 3.0f, colors::text);

        std::ostringstream value;
        value << std::fixed << std::setprecision(2) << value_;
        dl.text(Point{b.x + b.w * 0.5f, b.y + b.h - 22.0f}, value.str(), 12.0f,
                colors::textMuted, TextAlign::Center);
    }

private:
    std::string label_;
    State<float>& state_;
    float value_{};
    float minimum_{};
    float maximum_{1.0f};
    float drag_start_value_{};
    DragGesture drag_gesture_{};
    State<float>::Subscription subscription_;
};


class ToggleComponent final : public Component {
public:
    ToggleComponent(std::string label, State<bool>& state)
        : label_(std::move(label)), state_(state), value_(state.get()) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return Size{210.0f, 54.0f};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        subscription_ = state_.observe([this, invalidate = std::move(invalidate)](const bool& value) {
            value_ = value;
            invalidate();
        });
    }

    EventResult input(const InputEvent& event, InputContext& ctx) override {
        const bool keyboard_toggle = event.type == InputType::KeyDown &&
            (event.key == Key::Space || event.key == Key::Enter);
        const bool pointer_toggle = event.type == InputType::PointerDown;
        if (!keyboard_toggle && !pointer_toggle) return EventResult::Ignored;

        if (!effective_read_only()) {
            state_.set(!value_);
            ctx.invalidate();
        }
        return EventResult::Handled;
    }

    void paint(PaintContext& p) const override {
        const auto b = p.bounds();
        auto& dl = p.painter();
        dl.fill_rounded_rect(b, 12.0f, colors::panel);
        dl.stroke_rounded_rect(b, 12.0f, p.focused() ? 2.0f : 1.0f,
                               p.focused() ? colors::borderFocus : colors::border);
        dl.text(Point{b.x + 18.0f, b.y + b.h * 0.5f}, label_, 13.0f, colors::text, TextAlign::Left);
        const Rect track{b.x + b.w - 62.0f, b.y + 14.0f, 44.0f, 26.0f};
        dl.fill_rounded_rect(track, 13.0f, value_ ? colors::accent : colors::toggleOff);
        const float knob_x = value_ ? track.x + track.w - 13.0f : track.x + 13.0f;
        dl.circle(Point{knob_x, track.y + 13.0f}, 9.0f, colors::text);
    }

private:
    std::string label_;
    State<bool>& state_;
    bool value_{};
    State<bool>::Subscription subscription_;
};


