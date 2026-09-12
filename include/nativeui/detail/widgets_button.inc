#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/style.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

enum class ButtonVisualState {
    Normal,
    Hover,
    Pressed,
    Focused,
    Disabled,
};

namespace detail {

class ButtonComponent final : public Component, public ThemeBinding {
public:
    using ActivateCallback = std::function<void()>;

    ButtonComponent(std::string label, ActivateCallback on_activate, ButtonStyle style)
        : label_(std::move(label)),
          on_activate_(std::move(on_activate)),
          style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(focused_);
        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;
        const auto text = TextService::measure(label_, text_style);
        return Size{
            std::max(resolved.minimum_width,
                     text.width + resolved.horizontal_padding * 2.0f),
            resolved.control_height};
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = presentation_signature(focused_);
        focused_ = focused;
        interaction_.focus_changed(focused, context, false);
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, context);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = presentation_signature(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, context);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        const auto before = presentation_signature(focused_);
        const auto outcome = interaction_.input(event, context, true, false);
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, context);
        if (!outcome.activate) return outcome.result;

        // User code may synchronously disable/hide/remove this component. Copy
        // the callback only after the shared interaction machine has completed
        // all component/context work, then never touch `this` or `context`.
        auto callback = on_activate_;
        if (callback) callback();
        return outcome.result;
    }

    [[nodiscard]] ButtonVisualState visual_state(bool focused) const noexcept {
        if (!effective_enabled()) return ButtonVisualState::Disabled;
        if (interaction_.pressed()) return ButtonVisualState::Pressed;
        if (interaction_.hovered()) return ButtonVisualState::Hover;
        if (focused) return ButtonVisualState::Focused;
        return ButtonVisualState::Normal;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto resolved = resolved_style(context.focused());

        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.color = resolved.text;
        text_style.align = TextAlign::Center;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;

        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.fill);
        painter.stroke_rounded_rect(
            bounds, resolved.corner_radius, resolved.border_width, resolved.border);
        painter.text(
            Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            label_, text_style);
    }

private:
    struct PresentationSignature {
        Color fill{};
        Color border{};
        Color text{};
        float border_width{};
        float corner_radius{};
        float minimum_width{};
        float control_height{};
        float horizontal_padding{};
        float text_size{};
        FontWeight text_weight{FontWeight::Regular};
        FontSlant text_slant{FontSlant::Upright};
        std::string_view font_family;
        const std::vector<std::string>* fallback_families{};
    };

    static void apply_patch(PresentationSignature& target,
                            const ButtonStylePatch& patch) noexcept {
        if (patch.fill) target.fill = *patch.fill;
        if (patch.border) target.border = *patch.border;
        if (patch.text) target.text = *patch.text;
        if (patch.border_width) target.border_width = *patch.border_width;
        if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
        if (patch.minimum_width) target.minimum_width = *patch.minimum_width;
        if (patch.control_height) target.control_height = *patch.control_height;
        if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
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
            .border_width = theme.controls.border_width,
            .corner_radius = theme.radii.medium,
            .minimum_width = theme.controls.minimum_width,
            .control_height = theme.controls.control_height,
            .horizontal_padding = theme.spacing.large,
            .text_size = theme.typography.control_size,
            .text_weight = theme.typography.control_weight,
            .text_slant = theme.typography.slant,
            .font_family = theme.typography.family,
            .fallback_families = &theme.typography.fallback_families,
        };
        apply_patch(result, style_.base);

        const auto state = current_visual_state(focused);
        switch (resolve_interaction_state(state)) {
            case InteractionVisualState::Normal:
                break;
            case InteractionVisualState::Hovered:
                result.fill = theme.palette.control_hover;
                apply_patch(result, style_.hovered);
                break;
            case InteractionVisualState::Pressed:
                result.fill = theme.palette.accent;
                result.border = theme.palette.accent;
                result.text = theme.palette.background;
                apply_patch(result, style_.pressed);
                break;
            case InteractionVisualState::Disabled:
                result.fill = theme.palette.control_background;
                result.text = theme.palette.disabled;
                apply_patch(result, style_.disabled);
                break;
        }
        if (state.read_only) apply_patch(result, style_.read_only);
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
        return lhs.minimum_width == rhs.minimum_width &&
               lhs.control_height == rhs.control_height &&
               lhs.horizontal_padding == rhs.horizontal_padding &&
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
            .pressed = interaction_.pressed(),
            .focused = focused,
        };
    }

    [[nodiscard]] ResolvedButtonStyle resolved_style(bool focused) const {
        return resolve_button_style(
            default_button_style(current_theme()), style_, current_visual_state(focused));
    }

    std::string label_;
    ActivateCallback on_activate_;
    ButtonStyle style_;
    PressActivationState interaction_;
    bool focused_{};
};

} // namespace detail

class Button {
public:
    using ActivateCallback = detail::ButtonComponent::ActivateCallback;

    Button(std::string label, ActivateCallback on_activate)
        : label_(std::move(label)), on_activate_(std::move(on_activate)) {}

    Button&& style(ButtonStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto label = std::move(label_);
        auto callback = std::move(on_activate_);
        auto style = std::move(style_);
        return Spec{
            [label = std::move(label), callback = std::move(callback),
             style = std::move(style)]() mutable {
                return std::make_unique<detail::ButtonComponent>(
                    std::move(label), std::move(callback), std::move(style));
            },
            {}};
    }

private:
    std::string label_;
    ActivateCallback on_activate_;
    ButtonStyle style_;
};

} // namespace ui
