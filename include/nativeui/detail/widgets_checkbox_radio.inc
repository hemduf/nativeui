#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/state.hpp>
#include <nativeui/style.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

[[nodiscard]] inline bool read_only_value_input(const InputEvent& event) noexcept {
    if (event.type == InputType::PointerDown || event.type == InputType::PointerUp) return true;
    return (event.type == InputType::KeyDown || event.type == InputType::KeyUp) &&
           event.key == Key::Space;
}

class CheckboxComponent final : public Component, public ThemeBinding {
public:
    CheckboxComponent(State<bool>& state, std::string label, CheckboxStyle style)
        : state_(state), label_(std::move(label)), style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(false);
        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;
        const auto text = TextService::measure(label_, text_style);
        return Size{
            std::max(resolved.minimum_width,
                     text.width + resolved.leading_padding + resolved.box_size +
                         resolved.label_gap + 5.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        auto invalidate = context.invalidator();
        subscription_ = state_.observe(
            [invalidate = std::move(invalidate)](const bool&) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void focus_changed(bool focused, FocusContext& context) override {
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
        interaction_.deactivate(context);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (effective_read_only() && read_only_value_input(event)) {
            interaction_.cancel_pending_mutation(context);
            return EventResult::Handled;
        }

        auto* state = &state_;
        const auto outcome = interaction_.input(event, context, false);
        if (outcome.activate) state->set(!state->get());
        return outcome.result;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto resolved = resolved_style(context.focused());
        const float box_size = resolved.box_size;
        const Rect box{
            bounds.x + resolved.leading_padding,
            bounds.y + (bounds.h - box_size) * 0.5f,
            box_size,
            box_size};

        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.color = resolved.text;
        text_style.align = TextAlign::Left;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;

        auto& painter = context.painter();
        painter.fill_rounded_rect(box, resolved.box_corner_radius, resolved.box_fill);
        painter.stroke_rounded_rect(
            box, resolved.box_corner_radius, resolved.box_border_width,
            resolved.box_border);
        if (state_.get()) {
            constexpr float kReferenceBoxSize = 18.0f;
            const float scale = box_size / kReferenceBoxSize;
            painter.line(
                Point{box.x + 4.0f * scale, box.y + 9.0f * scale},
                Point{box.x + 8.0f * scale, box.y + 13.0f * scale},
                resolved.checkmark_width,
                resolved.checkmark);
            painter.line(
                Point{box.x + 8.0f * scale, box.y + 13.0f * scale},
                Point{box.x + 15.0f * scale, box.y + 5.0f * scale},
                resolved.checkmark_width,
                resolved.checkmark);
        }
        painter.text(
            Point{box.x + box.w + resolved.label_gap,
                  bounds.y + bounds.h * 0.5f},
            label_, text_style);
    }

private:
    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
            .checked = state_.get(),
        };
    }

    [[nodiscard]] ResolvedCheckboxStyle resolved_style(bool focused) const {
        return resolve_checkbox_style(
            default_checkbox_style(current_theme()), style_, current_visual_state(focused));
    }

    State<bool>& state_;
    std::string label_;
    CheckboxStyle style_;
    State<bool>::Subscription subscription_;
    PressActivationState interaction_;
};

struct RadioGroupToken final {};

template <class T>
class RadioOptionRegistry final {
public:
    [[nodiscard]] std::shared_ptr<const T> register_value(T value) {
        std::erase_if(active_values_, [](const std::weak_ptr<const T>& entry) {
            return entry.expired();
        });

        for (const auto& entry : active_values_) {
            if (auto existing = entry.lock(); existing && *existing == value) {
                throw std::invalid_argument(
                    "RadioButton option values must be unique within one RadioGroup");
            }
        }

        auto registered = std::make_shared<const T>(std::move(value));
        active_values_.push_back(registered);
        return registered;
    }

private:
    std::vector<std::weak_ptr<const T>> active_values_;
};

class RadioButtonComponent final : public Component,
                                   public FocusGroupParticipant,
                                   public ThemeBinding {
public:
    using IsSelected = std::function<bool()>;
    using Select = std::function<void()>;
    using Observe = std::function<std::shared_ptr<void>(std::function<void()>)>;

    RadioButtonComponent(
        std::shared_ptr<RadioGroupToken> group,
        std::string label,
        IsSelected is_selected,
        Select select,
        Observe observe,
        RadioStyle style)
        : group_(std::move(group)),
          label_(std::move(label)),
          is_selected_(std::move(is_selected)),
          select_(std::move(select)),
          observe_(std::move(observe)),
          style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(false);
        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;
        const auto text = TextService::measure(label_, text_style);
        return Size{
            std::max(resolved.minimum_width,
                     text.width + resolved.leading_padding +
                         resolved.outer_radius * 2.0f + resolved.label_gap + 6.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        if (!observe_) return;
        auto invalidate = context.invalidator();
        subscription_ = observe_([invalidate = std::move(invalidate)] { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void focus_changed(bool focused, FocusContext& context) override {
        interaction_.focus_changed(focused, context);
    }

    void deactivate(LifecycleContext& context) override {
        interaction_.deactivate(context);
    }

    [[nodiscard]] const void* focus_group_identity() const noexcept override {
        return group_.get();
    }

    [[nodiscard]] bool focus_group_selected() const override {
        return is_selected_ && is_selected_();
    }

    void focus_group_select() override {
        if (effective_read_only()) return;
        auto select = select_;
        if (select) select();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        if (effective_read_only() && read_only_value_input(event)) {
            interaction_.cancel_pending_mutation(context);
            return EventResult::Handled;
        }

        auto select = select_;
        const auto outcome = interaction_.input(event, context, false);
        if (outcome.activate && select) select();
        return outcome.result;
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto resolved = resolved_style(context.focused());
        const Point center{
            bounds.x + resolved.leading_padding + resolved.outer_radius,
            bounds.y + bounds.h * 0.5f};

        TextStyle text_style{};
        text_style.size = resolved.text_size;
        text_style.color = resolved.text;
        text_style.align = TextAlign::Left;
        text_style.weight = resolved.text_weight;
        text_style.slant = resolved.text_slant;
        text_style.family = resolved.font_family;
        text_style.fallback_families = resolved.fallback_families;

        auto& painter = context.painter();
        painter.circle(center, resolved.outer_radius, resolved.outer_fill);
        painter.circle(center, resolved.inner_radius, resolved.inner_fill);
        if (is_selected_ && is_selected_()) {
            painter.circle(center, resolved.mark_radius, resolved.mark_fill);
        }
        painter.text(
            Point{center.x + resolved.outer_radius + resolved.label_gap,
                  bounds.y + bounds.h * 0.5f},
            label_, text_style);
    }

private:
    [[nodiscard]] VisualState current_visual_state(bool focused) const {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
            .selected = is_selected_ && is_selected_(),
        };
    }

    [[nodiscard]] ResolvedRadioStyle resolved_style(bool focused) const {
        return resolve_radio_style(
            default_radio_style(current_theme()), style_, current_visual_state(focused));
    }

    std::shared_ptr<RadioGroupToken> group_;
    std::string label_;
    IsSelected is_selected_;
    Select select_;
    Observe observe_;
    RadioStyle style_;
    std::shared_ptr<void> subscription_;
    PressActivationState interaction_;
};

} // namespace detail

class Checkbox {
public:
    Checkbox(State<bool>& state, std::string label)
        : state_(&state), label_(std::move(label)) {}

    Checkbox&& style(CheckboxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* state = state_;
        auto label = std::move(label_);
        auto style = std::move(style_);
        return Spec{
            [state, label = std::move(label), style = std::move(style)]() mutable {
                return std::make_unique<detail::CheckboxComponent>(
                    *state, std::move(label), std::move(style));
            },
            {}};
    }

private:
    State<bool>* state_{};
    std::string label_;
    CheckboxStyle style_;
};

template <class T>
class RadioGroup {
public:
    explicit RadioGroup(State<T>& selected)
        : selected_(&selected),
          token_(std::make_shared<detail::RadioGroupToken>()),
          registry_(std::make_shared<detail::RadioOptionRegistry<T>>()) {}

private:
    template <class U>
    friend class RadioButton;

    State<T>* selected_{};
    std::shared_ptr<detail::RadioGroupToken> token_;
    std::shared_ptr<detail::RadioOptionRegistry<T>> registry_;
};

template <class T>
class RadioButton {
public:
    RadioButton(const RadioGroup<T>& group, T value, std::string label)
        : selected_(group.selected_),
          token_(group.token_),
          registry_(group.registry_),
          value_(std::move(value)),
          label_(std::move(label)) {}

    RadioButton&& style(RadioStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto* selected = selected_;
        auto token = std::move(token_);
        auto registry = std::move(registry_);
        auto value = std::move(value_);
        auto label = std::move(label_);
        auto style = std::move(style_);

        auto value_identity = registry->register_value(std::move(value));
        detail::RadioButtonComponent::IsSelected is_selected =
            [selected, value_identity] { return selected->get() == *value_identity; };
        detail::RadioButtonComponent::Select select =
            [selected, value_identity] { selected->set(*value_identity); };
        detail::RadioButtonComponent::Observe observe =
            [selected](std::function<void()> invalidate) -> std::shared_ptr<void> {
                auto subscription = selected->observe(
                    [invalidate = std::move(invalidate)](const T&) { invalidate(); });
                return std::make_shared<typename State<T>::Subscription>(std::move(subscription));
            };

        return Spec{
            [token = std::move(token),
             label = std::move(label),
             is_selected = std::move(is_selected),
             select = std::move(select),
             observe = std::move(observe),
             style = std::move(style)]() mutable {
                return std::make_unique<detail::RadioButtonComponent>(
                    std::move(token),
                    std::move(label),
                    std::move(is_selected),
                    std::move(select),
                    std::move(observe),
                    std::move(style));
            },
            {}};
    }

private:
    State<T>* selected_{};
    std::shared_ptr<detail::RadioGroupToken> token_;
    std::shared_ptr<detail::RadioOptionRegistry<T>> registry_;
    T value_;
    std::string label_;
    RadioStyle style_;
};

template <class T>
RadioButton(const RadioGroup<T>&, T, std::string) -> RadioButton<T>;

} // namespace ui
