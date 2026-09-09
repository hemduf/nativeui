#pragma once

#include <nativeui/component.hpp>

namespace ui::detail {

struct PressActivationResult {
    EventResult result{EventResult::Ignored};
    bool activate{};
};

// Shared Button-family pointer/keyboard state machine. The helper completes
// all component/context mutation before returning an activation request so the
// widget can invoke user/state callbacks without touching itself afterwards.
class PressActivationState {
public:
    [[nodiscard]] PressActivationResult input(
        const InputEvent& event, InputContext& context, bool allow_enter) {
        switch (event.type) {
            case InputType::PointerDown:
                pointer_armed_ = true;
                pointer_inside_ = true;
                hovered_ = true;
                context.capture_pointer();
                context.invalidate();
                return {EventResult::Handled, false};

            case InputType::PointerMove: {
                const bool inside = context.bounds().contains(event.position);
                const bool changed = hovered_ != inside ||
                                     (pointer_armed_ && pointer_inside_ != inside);
                hovered_ = inside;
                if (pointer_armed_) pointer_inside_ = inside;
                if (changed) context.invalidate();
                return {EventResult::Handled, false};
            }

            case InputType::PointerUp: {
                if (!pointer_armed_) return {EventResult::Ignored, false};
                const bool inside = context.bounds().contains(event.position);
                const bool activate = pointer_inside_ && inside;
                pointer_armed_ = false;
                pointer_inside_ = false;
                hovered_ = inside;
                context.release_pointer();
                context.invalidate();
                return {EventResult::Handled, activate};
            }

            case InputType::PointerCancel:
                if (!pointer_armed_) return {EventResult::Ignored, false};
                pointer_armed_ = false;
                pointer_inside_ = false;
                hovered_ = false;
                context.invalidate();
                return {EventResult::Handled, false};

            case InputType::KeyDown:
                if (event.key == Key::Space) {
                    if (!space_down_) {
                        space_down_ = true;
                        space_armed_ = true;
                        context.invalidate();
                    }
                    return {EventResult::Handled, false};
                }
                if (allow_enter && event.key == Key::Enter) {
                    if (enter_down_) return {EventResult::Handled, false};
                    enter_down_ = true;
                    return {EventResult::Handled, true};
                }
                return {EventResult::Ignored, false};

            case InputType::KeyUp:
                if (event.key == Key::Space) {
                    if (!space_down_) return {EventResult::Handled, false};
                    const bool activate = space_armed_;
                    space_down_ = false;
                    space_armed_ = false;
                    context.invalidate();
                    return {EventResult::Handled, activate};
                }
                if (allow_enter && event.key == Key::Enter) {
                    enter_down_ = false;
                    return {EventResult::Handled, false};
                }
                return {EventResult::Ignored, false};

            default:
                return {EventResult::Ignored, false};
        }
    }

    // ReadOnly value controls consume mutating input without starting or
    // retaining capture. If ReadOnly becomes effective while a press/Space
    // interaction is already armed, terminate that local interaction without
    // synthesizing PointerCancel (T059 owns cancellation for unavailability).
    void cancel_pending_mutation(InputContext& context) {
        const bool had_capture = pointer_armed_;
        const bool dirty = pointer_armed_ || pointer_inside_ || space_down_ || space_armed_ ||
                           enter_down_;
        pointer_armed_ = false;
        pointer_inside_ = false;
        space_down_ = false;
        space_armed_ = false;
        enter_down_ = false;
        if (had_capture) context.release_pointer();
        if (dirty) context.invalidate();
    }

    void focus_changed(bool focused, FocusContext& context) {
        if (focused) return;
        const bool dirty = pointer_armed_ || pointer_inside_ || hovered_ || space_down_ ||
                           space_armed_ || enter_down_;
        reset();
        if (dirty) context.invalidate();
    }

    void deactivate(LifecycleContext& context) {
        const bool dirty = pointer_armed_ || pointer_inside_ || hovered_ || space_down_ ||
                           space_armed_ || enter_down_;
        reset();
        if (dirty) context.invalidate();
    }

    [[nodiscard]] bool pressed() const noexcept {
        return (pointer_armed_ && pointer_inside_) || space_armed_;
    }
    [[nodiscard]] bool hovered() const noexcept { return hovered_; }

private:
    void reset() noexcept {
        pointer_armed_ = false;
        pointer_inside_ = false;
        hovered_ = false;
        space_down_ = false;
        space_armed_ = false;
        enter_down_ = false;
    }

    bool pointer_armed_{};
    bool pointer_inside_{};
    bool hovered_{};
    bool space_down_{};
    bool space_armed_{};
    bool enter_down_{};
};

} // namespace ui::detail
