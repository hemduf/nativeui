#pragma once

#include <nativeui/component.hpp>
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

class ButtonComponent final : public Component {
public:
    using ActivateCallback = std::function<void()>;

    ButtonComponent(std::string label, ActivateCallback on_activate)
        : label_(std::move(label)), on_activate_(std::move(on_activate)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto text = TextService::measure(label_, 13.0f);
        return Size{std::max(88.0f, text.width + 32.0f), 40.0f};
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

        Color fill = colors::panel;
        Color border = colors::border;
        Color text = colors::text;
        float border_width = 1.0f;

        switch (state) {
            case ButtonVisualState::Normal:
                break;
            case ButtonVisualState::Hover:
                fill = colors::knob;
                break;
            case ButtonVisualState::Pressed:
                fill = colors::accent;
                border = colors::accent;
                text = colors::background;
                break;
            case ButtonVisualState::Focused:
                border = colors::borderFocus;
                border_width = 2.0f;
                break;
            case ButtonVisualState::Disabled:
                fill = colors::input;
                text = colors::textMuted;
                break;
        }

        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, 8.0f, fill);
        painter.stroke_rounded_rect(bounds, 8.0f, border_width, border);
        painter.text(
            Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
            label_, 13.0f, text, TextAlign::Center);
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
