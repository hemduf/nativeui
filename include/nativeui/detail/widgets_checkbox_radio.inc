#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_widget_info.hpp>
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

[[nodiscard]] inline bool style_color_equal(Color lhs, Color rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] inline bool checkbox_layout_equal(const ResolvedCheckboxStyle& lhs,
                                                const ResolvedCheckboxStyle& rhs) {
    return lhs.box_size == rhs.box_size &&
           lhs.minimum_width == rhs.minimum_width &&
           lhs.control_height == rhs.control_height &&
           lhs.leading_padding == rhs.leading_padding &&
           lhs.label_gap == rhs.label_gap &&
           lhs.text_size == rhs.text_size &&
           lhs.text_weight == rhs.text_weight &&
           lhs.text_slant == rhs.text_slant &&
           lhs.font_family == rhs.font_family &&
           lhs.fallback_families == rhs.fallback_families;
}

[[nodiscard]] inline bool checkbox_presentation_equal(const ResolvedCheckboxStyle& lhs,
                                                      const ResolvedCheckboxStyle& rhs) {
    return checkbox_layout_equal(lhs, rhs) &&
           style_color_equal(lhs.box_fill, rhs.box_fill) &&
           style_color_equal(lhs.box_border, rhs.box_border) &&
           style_color_equal(lhs.checkmark, rhs.checkmark) &&
           style_color_equal(lhs.text, rhs.text) &&
           lhs.box_corner_radius == rhs.box_corner_radius &&
           lhs.box_border_width == rhs.box_border_width &&
           lhs.checkmark_width == rhs.checkmark_width;
}

[[nodiscard]] inline bool checkbox_checkmark_visible(
    const ResolvedCheckboxStyle& style) noexcept {
    return style.box_size > 0.0f && style.checkmark_width > 0.0f && style.checkmark.a > 0.0f;
}

[[nodiscard]] inline bool radio_layout_equal(const ResolvedRadioStyle& lhs,
                                             const ResolvedRadioStyle& rhs) {
    return lhs.outer_radius == rhs.outer_radius &&
           lhs.minimum_width == rhs.minimum_width &&
           lhs.control_height == rhs.control_height &&
           lhs.leading_padding == rhs.leading_padding &&
           lhs.label_gap == rhs.label_gap &&
           lhs.text_size == rhs.text_size &&
           lhs.text_weight == rhs.text_weight &&
           lhs.text_slant == rhs.text_slant &&
           lhs.font_family == rhs.font_family &&
           lhs.fallback_families == rhs.fallback_families;
}

[[nodiscard]] inline bool radio_presentation_equal(const ResolvedRadioStyle& lhs,
                                                   const ResolvedRadioStyle& rhs) {
    return radio_layout_equal(lhs, rhs) &&
           style_color_equal(lhs.outer_fill, rhs.outer_fill) &&
           style_color_equal(lhs.inner_fill, rhs.inner_fill) &&
           style_color_equal(lhs.mark_fill, rhs.mark_fill) &&
           style_color_equal(lhs.text, rhs.text) &&
           lhs.inner_radius == rhs.inner_radius &&
           lhs.mark_radius == rhs.mark_radius;
}

[[nodiscard]] inline bool radio_mark_visible(const ResolvedRadioStyle& style) noexcept {
    return style.mark_radius > 0.0f && style.mark_fill.a > 0.0f;
}

template <class Context, class Resolved, class SameLayout, class SamePresentation>
void invalidate_resolved_style_transition(const Resolved& before,
                                          const Resolved& after,
                                          Context& context,
                                          SameLayout&& same_layout,
                                          SamePresentation&& same_presentation) {
    if (same_presentation(before, after)) return;
    if (!same_layout(before, after)) {
        context.invalidate_layout();
        context.invalidate();
        return;
    }
    context.invalidate();
}

struct CheckboxStateInvalidation final {
    const Theme* theme{};
    CheckboxStyle style;
    VisualState visual{};
    std::function<void()> invalidate;
    std::function<void()> invalidate_layout;
    bool active{};

    void sync(VisualState state) noexcept { visual = state; }

    void publish(bool checked) {
        if (!active || !theme) return;

        auto before_state = visual;
        auto after_state = visual;
        after_state.checked = checked;

        const auto inherited = default_checkbox_style(*theme);
        const auto before = resolve_checkbox_style(inherited, style, before_state);
        const auto after = resolve_checkbox_style(inherited, style, after_state);
        const bool before_mark = before_state.checked && checkbox_checkmark_visible(before);
        const bool after_mark = after_state.checked && checkbox_checkmark_visible(after);

        visual = after_state;
        if (checkbox_presentation_equal(before, after) && before_mark == after_mark) return;
        if (!checkbox_layout_equal(before, after)) invalidate_layout();
        invalidate();
    }
};

struct RadioStateInvalidation final {
    const Theme* theme{};
    RadioStyle style;
    VisualState visual{};
    std::function<void()> invalidate;
    std::function<void()> invalidate_layout;
    bool active{};

    void sync(VisualState state) noexcept { visual = state; }

    void publish(bool selected) {
        if (!active || !theme) return;

        auto before_state = visual;
        auto after_state = visual;
        after_state.selected = selected;

        const auto inherited = default_radio_style(*theme);
        const auto before = resolve_radio_style(inherited, style, before_state);
        const auto after = resolve_radio_style(inherited, style, after_state);
        const bool before_mark = before_state.selected && radio_mark_visible(before);
        const bool after_mark = after_state.selected && radio_mark_visible(after);

        visual = after_state;
        if (radio_presentation_equal(before, after) && before_mark == after_mark) return;
        if (!radio_layout_equal(before, after)) invalidate_layout();
        invalidate();
    }
};

class CheckboxComponent final : public Component,
                                public ThemeBinding,
                                public SemanticActionHandler {
public:
    CheckboxComponent(Binding<bool> state, std::string label, CheckboxStyle style)
        : state_(std::move(state)), label_(std::move(label)), style_(std::move(style)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] SemanticInfo semantics() const override {
        return checkbox_semantic_info(label_, state_.get());
    }

    [[nodiscard]] bool perform_semantic_action(
        const SemanticActionRequest& request) override {
        if (request.action != SemanticAction::Toggle ||
            !effective_enabled() || effective_read_only()) {
            return false;
        }

        auto state = state_;
        const bool target = !state.get();
        state.set(target);
        return true;
    }

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
                     text.width + resolved.leading_padding + resolved.box_size +
                         resolved.label_gap + 5.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        // State/Binding notifications snapshot listeners before callbacks run.
        // Keep the classifier detached from the component so an earlier listener
        // may synchronously remove this node without leaving a raw `this` capture.
        state_invalidation_ = std::make_shared<CheckboxStateInvalidation>();
        state_invalidation_->theme = &current_theme();
        state_invalidation_->style = style_;
        state_invalidation_->invalidate = context.invalidator();
        state_invalidation_->invalidate_layout = context.layout_invalidator();
        state_invalidation_->active = true;
        sync_state_invalidation(effective_availability(), focused_);

        auto invalidation = state_invalidation_;
        subscription_ = state_.observe(
            [invalidation = std::move(invalidation)](const bool& checked) {
                invalidation->publish(checked);
            });
    }

    void unmount(LifecycleContext&) override {
        if (state_invalidation_) state_invalidation_->active = false;
        subscription_.reset();
        state_invalidation_.reset();
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = focused;
        interaction_.focus_changed(focused, context, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        const auto before = resolved_style(focused_);
        if (effective_read_only() && read_only_value_input(event)) {
            interaction_.cancel_pending_mutation(context, false);
            const auto after = resolved_style(focused_);
            invalidate_transition(before, after, context);
            sync_state_invalidation(effective_availability(), focused_);
            return EventResult::Handled;
        }

        auto state = state_;
        const auto outcome = interaction_.input(event, context, false, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
        if (!outcome.activate) return outcome.result;

        // Binding observers may synchronously rebuild/destroy this component.
        // Complete all component/context work before publishing the value and
        // never touch `this` afterwards.
        state.set(!state.get());
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
        return current_visual_state(effective_availability(), focused);
    }

    [[nodiscard]] VisualState current_visual_state(
        ComponentAvailability availability,
        bool focused) const noexcept {
        return VisualState{
            .enabled = availability.enabled,
            .read_only = availability.read_only,
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
            .checked = state_.get(),
        };
    }

    void sync_state_invalidation(
        ComponentAvailability availability,
        bool focused) const noexcept {
        if (state_invalidation_) {
            state_invalidation_->sync(current_visual_state(availability, focused));
        }
    }

    [[nodiscard]] ResolvedCheckboxStyle resolved_style(bool focused) const {
        return resolved_style(effective_availability(), focused);
    }

    [[nodiscard]] ResolvedCheckboxStyle resolved_style(
        ComponentAvailability availability,
        bool focused) const {
        return resolve_checkbox_style(
            default_checkbox_style(current_theme()),
            style_,
            current_visual_state(availability, focused));
    }

    [[nodiscard]] bool availability_change_affects_layout(
        const ComponentAvailability& before,
        const ComponentAvailability& after) const override {
        const bool changed = !checkbox_layout_equal(
            resolved_style(before, focused_),
            resolved_style(after, focused_));
        sync_state_invalidation(after, focused_);
        return changed;
    }

    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability& before,
        const ComponentAvailability& after) const noexcept override {
        const bool changed = !checkbox_presentation_equal(
            resolved_style(before, focused_),
            resolved_style(after, focused_));
        sync_state_invalidation(after, focused_);
        return changed;
    }

    template <class Context>
    static void invalidate_transition(const ResolvedCheckboxStyle& before,
                                      const ResolvedCheckboxStyle& after,
                                      Context& context) {
        invalidate_resolved_style_transition(
            before, after, context, checkbox_layout_equal, checkbox_presentation_equal);
    }

    Binding<bool> state_;
    std::string label_;
    CheckboxStyle style_;
    Binding<bool>::Subscription subscription_;
    std::shared_ptr<CheckboxStateInvalidation> state_invalidation_;
    PressActivationState interaction_;
    bool focused_{};
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
                                   public ThemeBinding,
                                   public SemanticActionHandler {
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

    [[nodiscard]] SemanticInfo semantics() const override {
        return radio_button_semantic_info(
            label_, is_selected_ && is_selected_());
    }

    [[nodiscard]] bool perform_semantic_action(
        const SemanticActionRequest& request) override {
        if (request.action != SemanticAction::Select ||
            !effective_enabled() || effective_read_only()) {
            return false;
        }

        auto select = select_;
        if (!select) return false;
        select();
        return true;
    }

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
                     text.width + resolved.leading_padding +
                         resolved.outer_radius * 2.0f + resolved.label_gap + 6.0f),
            resolved.control_height};
    }

    void mount(MountContext& context) override {
        if (!observe_) return;

        state_invalidation_ = std::make_shared<RadioStateInvalidation>();
        state_invalidation_->theme = &current_theme();
        state_invalidation_->style = style_;
        state_invalidation_->invalidate = context.invalidator();
        state_invalidation_->invalidate_layout = context.layout_invalidator();
        state_invalidation_->active = true;
        sync_state_invalidation(effective_availability(), focused_);

        auto invalidation = state_invalidation_;
        auto is_selected = is_selected_;
        subscription_ = observe_(
            [invalidation = std::move(invalidation),
             is_selected = std::move(is_selected)]() {
                invalidation->publish(is_selected && is_selected());
            });
    }

    void unmount(LifecycleContext&) override {
        if (state_invalidation_) state_invalidation_->active = false;
        subscription_.reset();
        state_invalidation_.reset();
    }

    void focus_changed(bool focused, FocusContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = focused;
        interaction_.focus_changed(focused, context, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
    }

    void deactivate(LifecycleContext& context) override {
        const auto before = resolved_style(focused_);
        focused_ = false;
        interaction_.deactivate(context, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
    }

    [[nodiscard]] const void* focus_group_identity() const noexcept override {
        return group_.get();
    }

    [[nodiscard]] bool focus_group_selected() const override {
        return is_selected_ && is_selected_();
    }

    void focus_group_select() override {
        if (effective_read_only()) return;
        sync_state_invalidation(effective_availability(), focused_);
        auto select = select_;
        if (select) select();
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        const auto before = resolved_style(focused_);
        if (effective_read_only() && read_only_value_input(event)) {
            interaction_.cancel_pending_mutation(context, false);
            const auto after = resolved_style(focused_);
            invalidate_transition(before, after, context);
            sync_state_invalidation(effective_availability(), focused_);
            return EventResult::Handled;
        }

        auto select = select_;
        const auto outcome = interaction_.input(event, context, false, false);
        const auto after = resolved_style(focused_);
        invalidate_transition(before, after, context);
        sync_state_invalidation(effective_availability(), focused_);
        if (!outcome.activate) return outcome.result;

        // Selection callbacks may rebuild the retained tree. Do not access the
        // component or InputContext after invoking user/application state.
        if (select) select();
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
        return current_visual_state(effective_availability(), focused);
    }

    [[nodiscard]] VisualState current_visual_state(
        ComponentAvailability availability,
        bool focused) const {
        return VisualState{
            .enabled = availability.enabled,
            .read_only = availability.read_only,
            .hovered = interaction_.hovered(),
            .pressed = interaction_.pressed(),
            .focused = focused,
            .selected = is_selected_ && is_selected_(),
        };
    }

    void sync_state_invalidation(
        ComponentAvailability availability,
        bool focused) const noexcept {
        if (state_invalidation_) {
            state_invalidation_->sync(current_visual_state(availability, focused));
        }
    }

    [[nodiscard]] ResolvedRadioStyle resolved_style(bool focused) const {
        return resolved_style(effective_availability(), focused);
    }

    [[nodiscard]] ResolvedRadioStyle resolved_style(
        ComponentAvailability availability,
        bool focused) const {
        return resolve_radio_style(
            default_radio_style(current_theme()),
            style_,
            current_visual_state(availability, focused));
    }

    [[nodiscard]] bool availability_change_affects_layout(
        const ComponentAvailability& before,
        const ComponentAvailability& after) const override {
        const bool changed = !radio_layout_equal(
            resolved_style(before, focused_),
            resolved_style(after, focused_));
        sync_state_invalidation(after, focused_);
        return changed;
    }

    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability& before,
        const ComponentAvailability& after) const noexcept override {
        const bool changed = !radio_presentation_equal(
            resolved_style(before, focused_),
            resolved_style(after, focused_));
        sync_state_invalidation(after, focused_);
        return changed;
    }

    template <class Context>
    static void invalidate_transition(const ResolvedRadioStyle& before,
                                      const ResolvedRadioStyle& after,
                                      Context& context) {
        invalidate_resolved_style_transition(
            before, after, context, radio_layout_equal, radio_presentation_equal);
    }

    std::shared_ptr<RadioGroupToken> group_;
    std::string label_;
    IsSelected is_selected_;
    Select select_;
    Observe observe_;
    RadioStyle style_;
    std::shared_ptr<void> subscription_;
    std::shared_ptr<RadioStateInvalidation> state_invalidation_;
    PressActivationState interaction_;
    bool focused_{};
};

} // namespace detail

class Checkbox {
public:
    Checkbox(Binding<bool> state, std::string label)
        : state_(std::move(state)), label_(std::move(label)) {}

    Checkbox(State<bool>& state, std::string label)
        : Checkbox(state.binding(), std::move(label)) {}

    Checkbox&& style(CheckboxStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    Spec spec() && {
        auto state = std::move(state_);
        auto label = std::move(label_);
        auto style = std::move(style_);
        return Spec{
            [state = std::move(state),
             label = std::move(label),
             style = std::move(style)]() mutable {
                return std::make_unique<detail::CheckboxComponent>(
                    std::move(state), std::move(label), std::move(style));
            },
            {}};
    }

private:
    Binding<bool> state_;
    std::string label_;
    CheckboxStyle style_;
};

template <class T>
class RadioGroup {
public:
    explicit RadioGroup(Binding<T> selected)
        : selected_(std::move(selected)),
          token_(std::make_shared<detail::RadioGroupToken>()),
          registry_(std::make_shared<detail::RadioOptionRegistry<T>>()) {}

    explicit RadioGroup(State<T>& selected)
        : RadioGroup(selected.binding()) {}

private:
    template <class U>
    friend class RadioButton;

    Binding<T> selected_;
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
        auto selected = std::move(selected_);
        auto token = std::move(token_);
        auto registry = std::move(registry_);
        auto value = std::move(value_);
        auto label = std::move(label_);
        auto style = std::move(style_);

        auto value_identity = registry->register_value(std::move(value));
        detail::RadioButtonComponent::IsSelected is_selected =
            [selected, value_identity] { return selected.get() == *value_identity; };
        detail::RadioButtonComponent::Select select =
            [selected, value_identity]() mutable { selected.set(*value_identity); };
        detail::RadioButtonComponent::Observe observe =
            [selected](std::function<void()> invalidate) mutable -> std::shared_ptr<void> {
                auto subscription = selected.observe(
                    [invalidate = std::move(invalidate)](const T&) { invalidate(); });
                return std::make_shared<typename Binding<T>::Subscription>(std::move(subscription));
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
    Binding<T> selected_;
    std::shared_ptr<detail::RadioGroupToken> token_;
    std::shared_ptr<detail::RadioOptionRegistry<T>> registry_;
    T value_;
    std::string label_;
    RadioStyle style_;
};

template <class T>
RadioButton(const RadioGroup<T>&, T, std::string) -> RadioButton<T>;

} // namespace ui
