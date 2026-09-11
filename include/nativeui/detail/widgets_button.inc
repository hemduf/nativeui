#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
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

    ButtonComponent(std::string label, ActivateCallback on_activate)
        : label_(std::move(label)), on_activate_(std::move(on_activate)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto& theme = current_theme();
        TextStyle style{};
        style.size = theme.typography.control_size;
        style.weight = theme.typography.control_weight;
        style.slant = theme.typography.slant;
        style.family = theme.typography.family;
        style.fallback_families = theme.typography.fallback_families;
        const auto text = TextService::measure(label_, style);
        return Size{
            std::max(theme.controls.minimum_width, text.width + theme.spacing.large * 2.0f),
            theme.controls.control_height};
    }

    void focus_changed(bool focused, FocusContext& context) override {
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
        interaction_.deactivate(context);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        const auto outcome = interaction_.input(event, context, true);
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
        const auto state = visual_state(context.focused());
        const auto& theme = current_theme();

        Color fill = theme.palette.surface;
        Color border = theme.palette.border;
        Color text = theme.palette.text;
        float border_width = theme.controls.border_width;

        switch (state) {
            case ButtonVisualState::Normal:
                break;
            case ButtonVisualState::Hover:
                fill = theme.palette.control_hover;
                break;
            case ButtonVisualState::Pressed:
                fill = theme.palette.accent;
                border = theme.palette.accent;
                text = theme.palette.background;
                break;
            case ButtonVisualState::Focused:
                border = theme.palette.focus;
                border_width = theme.controls.focus_ring_width;
                break;
            case ButtonVisualState::Disabled:
                fill = theme.palette.control_background;
                text = theme.palette.disabled;
                break;
        }

        TextStyle text_style{};
        text_style.size = theme.typography.control_size;
        text_style.color = text;
        text_style.align = TextAlign::Center;
        text_style.weight = theme.typography.control_weight;
        text_style.slant = theme.typography.slant;
        text_style.family = theme.typography.family;
        text_style.fallback_families = theme.typography.fallback_families;

        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, theme.radii.medium, fill);
        painter.stroke_rounded_rect(bounds, theme.radii.medium, border_width, border);
        painter.text(
            Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            label_, text_style);
    }

private:
    std::string label_;
    ActivateCallback on_activate_;
    PressActivationState interaction_;
};

} // namespace detail

class Button {
public:
    using ActivateCallback = detail::ButtonComponent::ActivateCallback;

    Button(std::string label, ActivateCallback on_activate)
        : label_(std::move(label)), on_activate_(std::move(on_activate)) {}

    Spec spec() && {
        auto label = std::move(label_);
        auto callback = std::move(on_activate_);
        return Spec{
            [label = std::move(label), callback = std::move(callback)]() mutable {
                return std::make_unique<detail::ButtonComponent>(
                    std::move(label), std::move(callback));
            },
            {}};
    }

private:
    std::string label_;
    ActivateCallback on_activate_;
};

} // namespace ui
