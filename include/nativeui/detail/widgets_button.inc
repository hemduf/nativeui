#pragma once

#include <nativeui/component.hpp>

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
        if (focused) return;
        if (!pointer_armed_ && !space_down_ && !space_armed_ && !enter_down_ && !hovered_) {
            return;
        }
        pointer_armed_ = false;
        pointer_inside_ = false;
        hovered_ = false;
        space_down_ = false;
        space_armed_ = false;
        enter_down_ = false;
        context.invalidate();
    }

    void deactivate(LifecycleContext& context) override {
        const bool dirty = pointer_armed_ || pointer_inside_ || hovered_ || space_down_ ||
                           space_armed_ || enter_down_;
        pointer_armed_ = false;
        pointer_inside_ = false;
        hovered_ = false;
        space_down_ = false;
        space_armed_ = false;
        enter_down_ = false;
        if (dirty) context.invalidate();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        switch (event.type) {
            case InputType::PointerDown:
                pointer_armed_ = true;
                pointer_inside_ = true;
                hovered_ = true;
                context.capture_pointer();
                context.invalidate();
                return EventResult::Handled;

            case InputType::PointerMove: {
                const bool inside = context.bounds().contains(event.position);
                const bool changed = hovered_ != inside ||
                                     (pointer_armed_ && pointer_inside_ != inside);
                hovered_ = inside;
                if (pointer_armed_) pointer_inside_ = inside;
                if (changed) context.invalidate();
                return EventResult::Handled;
            }

            case InputType::PointerUp: {
                if (!pointer_armed_) return EventResult::Ignored;
                const bool inside = context.bounds().contains(event.position);
                const bool activate = pointer_inside_ && inside;
                auto callback = activate ? on_activate_ : ActivateCallback{};

                pointer_armed_ = false;
                pointer_inside_ = false;
                hovered_ = inside;
                context.release_pointer();
                context.invalidate();

                // The activation callback may synchronously disable/hide/remove
                // this Button. Invoke a detached callback copy only after all
                // component/context work is complete and do not touch `this` or
                // `context` after the call.
                if (callback) callback();
                return EventResult::Handled;
            }

            case InputType::PointerCancel:
                if (!pointer_armed_) return EventResult::Ignored;
                pointer_armed_ = false;
                pointer_inside_ = false;
                hovered_ = false;
                context.invalidate();
                return EventResult::Handled;

            case InputType::KeyDown:
                if (event.key == Key::Space) {
                    if (!space_down_) {
                        space_down_ = true;
                        space_armed_ = true;
                        context.invalidate();
                    }
                    return EventResult::Handled;
                }
                if (event.key == Key::Enter) {
                    if (enter_down_) return EventResult::Handled;
                    enter_down_ = true;
                    auto callback = on_activate_;
                    if (callback) callback();
                    return EventResult::Handled;
                }
                return EventResult::Ignored;

            case InputType::KeyUp:
                if (event.key == Key::Space) {
                    if (!space_down_) return EventResult::Handled;
                    const bool activate = space_armed_;
                    auto callback = activate ? on_activate_ : ActivateCallback{};
                    space_down_ = false;
                    space_armed_ = false;
                    context.invalidate();
                    if (callback) callback();
                    return EventResult::Handled;
                }
                if (event.key == Key::Enter) {
                    enter_down_ = false;
                    return EventResult::Handled;
                }
                return EventResult::Ignored;

            default:
                return EventResult::Ignored;
        }
    }

    [[nodiscard]] ButtonVisualState visual_state(bool focused) const noexcept {
        if (!effective_enabled()) return ButtonVisualState::Disabled;
        if ((pointer_armed_ && pointer_inside_) || space_armed_) {
            return ButtonVisualState::Pressed;
        }
        if (hovered_) return ButtonVisualState::Hover;
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
    bool pointer_armed_{};
    bool pointer_inside_{};
    bool hovered_{};
    bool space_down_{};
    bool space_armed_{};
    bool enter_down_{};
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
