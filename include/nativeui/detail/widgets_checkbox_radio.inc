#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/semantic_widget_info.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/state.hpp>

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

class CheckboxComponent final : public Component {
public:
    CheckboxComponent(State<bool>& state, std::string label)
        : state_(state), label_(std::move(label)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] SemanticInfo semantics() const override {
        return checkbox_semantic_info(label_, state_.get());
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto text = TextService::measure(label_, 13.0f);
        return Size{std::max(72.0f, text.width + 36.0f), 32.0f};
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
        const bool checked = state_.get();
        const bool disabled = !effective_enabled();
        const float box_size = 18.0f;
        const Rect box{
            bounds.x + 3.0f,
            bounds.y + (bounds.h - box_size) * 0.5f,
            box_size,
            box_size};

        Color fill = checked ? colors::accent : colors::input;
        Color border = context.focused() ? colors::borderFocus : colors::border;
        const Color text = disabled ? colors::textMuted : colors::text;
        if (disabled) fill = colors::input;
        if (interaction_.pressed() && !disabled) border = colors::accent;

        auto& painter = context.painter();
        painter.fill_rounded_rect(box, 4.0f, fill);
        painter.stroke_rounded_rect(box, 4.0f, context.focused() ? 2.0f : 1.0f, border);
        if (checked) {
            const Color mark = disabled ? colors::textMuted : colors::background;
            painter.line(
                Point{box.x + 4.0f, box.y + 9.0f},
                Point{box.x + 8.0f, box.y + 13.0f}, 2.0f, mark);
            painter.line(
                Point{box.x + 8.0f, box.y + 13.0f},
                Point{box.x + 15.0f, box.y + 5.0f}, 2.0f, mark);
        }
        painter.text(
            Point{box.x + box.w + 10.0f, bounds.y + bounds.h * 0.5f},
            label_, 13.0f, text, TextAlign::Left);
    }

private:
    State<bool>& state_;
    std::string label_;
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
    // Weak entries model only simultaneously-live option specifications/components.
    // Once an earlier UI tree releases its option value, the same group/value can
    // be used again by a remounted or reordered tree without retaining stale state.
    std::vector<std::weak_ptr<const T>> active_values_;
};

class RadioButtonComponent final : public Component, public FocusGroupParticipant {
public:
    using IsSelected = std::function<bool()>;
    using Select = std::function<void()>;
    using Observe = std::function<std::shared_ptr<void>(std::function<void()>)>;

    RadioButtonComponent(
        std::shared_ptr<RadioGroupToken> group,
        std::string label,
        IsSelected is_selected,
        Select select,
        Observe observe)
        : group_(std::move(group)),
          label_(std::move(label)),
          is_selected_(std::move(is_selected)),
          select_(std::move(select)),
          observe_(std::move(observe)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] SemanticInfo semantics() const override {
        return radio_button_semantic_info(
            label_, is_selected_ && is_selected_());
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto text = TextService::measure(label_, 13.0f);
        return Size{std::max(72.0f, text.width + 36.0f), 32.0f};
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
        const bool selected = is_selected_ && is_selected_();
        const bool disabled = !effective_enabled();
        const Point center{bounds.x + 12.0f, bounds.y + bounds.h * 0.5f};

        Color outer = context.focused() ? colors::borderFocus : colors::border;
        const Color inner = colors::input;
        const Color text = disabled ? colors::textMuted : colors::text;
        if (interaction_.pressed() && !disabled) outer = colors::accent;
        if (disabled) outer = colors::border;

        auto& painter = context.painter();
        painter.circle(center, 9.0f, outer);
        painter.circle(center, 7.0f, inner);
        if (selected) {
            painter.circle(center, 4.0f, disabled ? colors::textMuted : colors::accent);
        }
        painter.text(
            Point{center.x + 18.0f, bounds.y + bounds.h * 0.5f},
            label_, 13.0f, text, TextAlign::Left);
    }

private:
    std::shared_ptr<RadioGroupToken> group_;
    std::string label_;
    IsSelected is_selected_;
    Select select_;
    Observe observe_;
    std::shared_ptr<void> subscription_;
    PressActivationState interaction_;
};

} // namespace detail

class Checkbox {
public:
    Checkbox(State<bool>& state, std::string label)
        : state_(&state), label_(std::move(label)) {}

    Spec spec() && {
        auto* state = state_;
        auto label = std::move(label_);
        return Spec{
            [state, label = std::move(label)]() mutable {
                return std::make_unique<detail::CheckboxComponent>(*state, std::move(label));
            },
            {}};
    }

private:
    State<bool>* state_{};
    std::string label_;
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

    Spec spec() && {
        auto* selected = selected_;
        auto token = std::move(token_);
        auto registry = std::move(registry_);
        auto value = std::move(value_);
        auto label = std::move(label_);

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
             observe = std::move(observe)]() mutable {
                return std::make_unique<detail::RadioButtonComponent>(
                    std::move(token),
                    std::move(label),
                    std::move(is_selected),
                    std::move(select),
                    std::move(observe));
            },
            {}};
    }

private:
    State<T>* selected_{};
    std::shared_ptr<detail::RadioGroupToken> token_;
    std::shared_ptr<detail::RadioOptionRegistry<T>> registry_;
    T value_;
    std::string label_;
};

template <class T>
RadioButton(const RadioGroup<T>&, T, std::string) -> RadioButton<T>;

} // namespace ui
