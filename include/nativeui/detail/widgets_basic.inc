#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>
#include <nativeui/toggle_style.hpp>

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


class ToggleComponent final : public Component, public detail::ThemeBinding {
public:
    ToggleComponent(std::string label, State<bool>& state, ToggleStyle style)
        : label_(std::move(label)),
          state_(state),
          value_(state.get()),
          style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(focused_);
        return Size{resolved.control_width, resolved.control_height};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        auto invalidate_layout = ctx.layout_invalidator();
        subscription_ = state_.observe(
            [this,
             invalidate = std::move(invalidate),
             invalidate_layout = std::move(invalidate_layout)](const bool& value) {
                const auto before = presentation_signature(focused_);
                value_ = value;
                const auto after = presentation_signature(focused_);
                if (same_presentation(before, after)) return;
                if (!same_layout(before, after)) {
                    invalidate_layout();
                    return;
                }
                invalidate();
            });
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = presentation_signature(focused_);
        focused_ = focused;
        interaction_.focus_changed(focused, context, false);
        if (!focused) {
            space_pressed_ = false;
            enter_pressed_ = false;
        }
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, context);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = presentation_signature(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        space_pressed_ = false;
        enter_pressed_ = false;
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, context);
    }

    EventResult input(const InputEvent& event, InputContext& ctx) override {
        const auto before = presentation_signature(focused_);
        const bool pointer_event = event.type == InputType::PointerDown ||
            event.type == InputType::PointerMove || event.type == InputType::PointerUp ||
            event.type == InputType::PointerCancel;

        if (pointer_event) {
            const bool mutating_pointer = event.type == InputType::PointerDown ||
                interaction_.pressed();
            if (effective_read_only() && mutating_pointer) {
                interaction_.cancel_pending_mutation(ctx, false);
                const auto after = presentation_signature(focused_);
                invalidate_style_transition(before, after, ctx);
                return EventResult::Handled;
            }

            const auto outcome = interaction_.input(event, ctx, false, false);
            if (event.type == InputType::PointerDown && !effective_read_only()) {
                state_.set(!value_);
            }
            const auto after = presentation_signature(focused_);
            invalidate_style_transition(before, after, ctx);
            return outcome.result;
        }

        if (event.type == InputType::KeyDown && event.key == Key::Space) {
            if (effective_read_only()) return EventResult::Handled;
            if (!space_pressed_) {
                space_pressed_ = true;
                state_.set(!value_);
                const auto after = presentation_signature(focused_);
                invalidate_style_transition(before, after, ctx);
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyDown && event.key == Key::Enter) {
            if (effective_read_only()) return EventResult::Handled;
            if (!enter_pressed_) {
                enter_pressed_ = true;
                state_.set(!value_);
                const auto after = presentation_signature(focused_);
                invalidate_style_transition(before, after, ctx);
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyUp && event.key == Key::Space) {
            if (space_pressed_) {
                space_pressed_ = false;
                const auto after = presentation_signature(focused_);
                invalidate_style_transition(before, after, ctx);
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyUp && event.key == Key::Enter) {
            if (enter_pressed_) {
                enter_pressed_ = false;
                const auto after = presentation_signature(focused_);
                invalidate_style_transition(before, after, ctx);
            }
            return EventResult::Handled;
        }

        return EventResult::Ignored;
    }

    void paint(PaintContext& p) const override {
        const auto b = p.bounds();
        const auto resolved = resolved_style(p.focused());
        auto& dl = p.painter();

        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.color = resolved.text;
        text_style.align = TextAlign::Left;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;

        dl.fill_rounded_rect(b, resolved.corner_radius, resolved.fill);
        dl.stroke_rounded_rect(
            b, resolved.corner_radius, resolved.border_width, resolved.border);
        dl.text(Point{b.x + resolved.leading_padding, b.y + b.h * 0.5f},
                label_, text_style);

        const Rect track{
            b.x + b.w - resolved.trailing_padding - resolved.track_width,
            b.y + (resolved.control_height - resolved.track_height) * 0.5f,
            resolved.track_width,
            resolved.track_height};
        const float track_radius = resolved.track_height * 0.5f;
        dl.fill_rounded_rect(track, track_radius, resolved.track);

        const float thumb_radius = resolved.thumb_diameter * 0.5f;
        const float thumb_x = value_
            ? track.x + track.w - track_radius
            : track.x + track_radius;
        dl.circle(Point{thumb_x, track.y + track_radius}, thumb_radius, resolved.thumb);
    }

private:
    struct PresentationSignature {
        Color fill{};
        Color border{};
        Color text{};
        Color track{};
        Color thumb{};
        float border_width{};
        float corner_radius{};
        float control_width{};
        float control_height{};
        float leading_padding{};
        float trailing_padding{};
        float track_width{};
        float track_height{};
        float thumb_diameter{};
        float text_size{};
        FontWeight text_weight{FontWeight::Regular};
        FontSlant text_slant{FontSlant::Upright};
        std::string_view font_family;
        const std::vector<std::string>* fallback_families{};
    };

    static void apply_patch(PresentationSignature& target,
                            const ToggleStylePatch& patch) noexcept {
        if (patch.fill) target.fill = *patch.fill;
        if (patch.border) target.border = *patch.border;
        if (patch.text) target.text = *patch.text;
        if (patch.track) target.track = *patch.track;
        if (patch.thumb) target.thumb = *patch.thumb;
        if (patch.border_width) target.border_width = *patch.border_width;
        if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
        if (patch.control_width) target.control_width = *patch.control_width;
        if (patch.control_height) target.control_height = *patch.control_height;
        if (patch.leading_padding) target.leading_padding = *patch.leading_padding;
        if (patch.trailing_padding) target.trailing_padding = *patch.trailing_padding;
        if (patch.track_width) target.track_width = *patch.track_width;
        if (patch.track_height) target.track_height = *patch.track_height;
        if (patch.thumb_diameter) target.thumb_diameter = *patch.thumb_diameter;
        if (patch.text_size) target.text_size = *patch.text_size;
        if (patch.text_weight) target.text_weight = *patch.text_weight;
        if (patch.text_slant) target.text_slant = *patch.text_slant;
        if (patch.font_family) target.font_family = *patch.font_family;
        if (patch.fallback_families) target.fallback_families = &*patch.fallback_families;
    }

    [[nodiscard]] PresentationSignature presentation_signature(bool focused) const noexcept {
        const auto& theme = current_theme();
        PresentationSignature result{
            .fill = theme.palette.surface,
            .border = theme.palette.border,
            .text = theme.palette.text,
            .track = colors::toggleOff,
            .thumb = theme.palette.text,
            .border_width = theme.controls.border_width,
            .corner_radius = theme.radii.large,
            .control_width = 210.0f,
            .control_height = 54.0f,
            .leading_padding = 18.0f,
            .trailing_padding = 18.0f,
            .track_width = 44.0f,
            .track_height = 26.0f,
            .thumb_diameter = 18.0f,
            .text_size = theme.typography.control_size,
            .text_weight = theme.typography.control_weight,
            .text_slant = theme.typography.slant,
            .font_family = theme.typography.family,
            .fallback_families = &theme.typography.fallback_families,
        };
        apply_patch(result, style_.base);

        const auto state = current_visual_state(focused);
        if (state.checked) {
            result.track = theme.palette.accent;
            apply_patch(result, style_.checked);
        }

        switch (resolve_interaction_state(state)) {
            case InteractionVisualState::Normal:
                break;
            case InteractionVisualState::Hovered:
                result.border = theme.palette.control_hover;
                apply_patch(result, style_.hovered);
                break;
            case InteractionVisualState::Pressed:
                result.border = theme.palette.accent;
                result.thumb = theme.palette.active_highlight;
                apply_patch(result, style_.pressed);
                break;
            case InteractionVisualState::Disabled:
                result.fill = theme.palette.control_background;
                result.border = theme.palette.disabled;
                result.text = theme.palette.disabled;
                apply_patch(result, style_.disabled);
                break;
        }
        if (state.read_only) {
            result.border = theme.palette.track;
            result.text = theme.palette.muted_text;
            apply_patch(result, style_.read_only);
        }
        if (state.focused) {
            result.border = theme.palette.focus;
            result.border_width = theme.controls.focus_ring_width;
            apply_patch(result, style_.focused);
        }
        return result;
    }

    [[nodiscard]] static bool same_color(Color lhs, Color rhs) noexcept {
        return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
    }

    [[nodiscard]] static bool same_fallbacks(const PresentationSignature& lhs,
                                             const PresentationSignature& rhs) {
        return lhs.fallback_families == rhs.fallback_families ||
            (lhs.fallback_families != nullptr && rhs.fallback_families != nullptr &&
             *lhs.fallback_families == *rhs.fallback_families);
    }

    [[nodiscard]] static bool same_layout(const PresentationSignature& lhs,
                                          const PresentationSignature& rhs) {
        return lhs.control_width == rhs.control_width &&
               lhs.control_height == rhs.control_height &&
               lhs.leading_padding == rhs.leading_padding &&
               lhs.trailing_padding == rhs.trailing_padding &&
               lhs.track_width == rhs.track_width &&
               lhs.track_height == rhs.track_height &&
               lhs.thumb_diameter == rhs.thumb_diameter &&
               lhs.text_size == rhs.text_size &&
               lhs.text_weight == rhs.text_weight &&
               lhs.text_slant == rhs.text_slant &&
               lhs.font_family == rhs.font_family && same_fallbacks(lhs, rhs);
    }

    [[nodiscard]] static bool same_presentation(const PresentationSignature& lhs,
                                                const PresentationSignature& rhs) {
        return same_color(lhs.fill, rhs.fill) &&
               same_color(lhs.border, rhs.border) &&
               same_color(lhs.text, rhs.text) &&
               same_color(lhs.track, rhs.track) &&
               same_color(lhs.thumb, rhs.thumb) &&
               lhs.border_width == rhs.border_width &&
               lhs.corner_radius == rhs.corner_radius &&
               same_layout(lhs, rhs);
    }

    template <class Context>
    static void invalidate_style_transition(const PresentationSignature& before,
                                            const PresentationSignature& after,
                                            Context& context) {
        if (same_presentation(before, after)) return;
        if (!same_layout(before, after)) {
            context.invalidate_layout();
            return;
        }
        context.invalidate();
    }

    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed() || space_pressed_ || enter_pressed_,
            .focused = focused,
            .checked = value_,
        };
    }

    [[nodiscard]] ResolvedToggleStyle resolved_style(bool focused) const {
        return resolve_toggle_style(
            default_toggle_style(current_theme()), style_, current_visual_state(focused));
    }

    std::string label_;
    State<bool>& state_;
    bool value_{};
    ToggleStyle style_;
    detail::PressActivationState interaction_;
    bool focused_{};
    bool space_pressed_{};
    bool enter_pressed_{};
    State<bool>::Subscription subscription_;
};

