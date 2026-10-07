#pragma once

#include <nativeui/detail/checkbox_radio_helpers.hpp>
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

struct RadioGroupToken final {};

template <class T> class RadioOptionRegistry final {
public:
  [[nodiscard]] std::shared_ptr<const T> register_value(T value) {
    std::erase_if(active_values_, [](const std::weak_ptr<const T> &entry) {
      return entry.expired();
    });

    for (const auto &entry : active_values_) {
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
  using IsValid = std::function<bool()>;
  using Select = std::function<void()>;
  using Observe = std::function<std::shared_ptr<void>(std::function<void()>)>;

  RadioButtonComponent(std::shared_ptr<RadioGroupToken> group,
                       std::string label, IsSelected is_selected, Select select,
                       Observe observe, RadioStyle style);

  RadioButtonComponent(std::shared_ptr<RadioGroupToken> group,
                       std::string label, IsSelected is_selected, Select select,
                       Observe observe, RadioStyle style, IsValid is_valid);

  void set_selection_revision(std::function<std::uint64_t()> reader);
  [[nodiscard]] std::uint64_t focus_group_selection_revision() const noexcept override;
  void set_guarded_select(std::function<void(const std::function<bool()>&)> select);
  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override;
  EventResult semantic_action(SemanticAction action, InputContext &context) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &context) override;

  void unmount(LifecycleContext &) override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  [[nodiscard]] const void *focus_group_identity() const noexcept override;

  [[nodiscard]] bool focus_group_selected() const override;
  [[nodiscard]] bool
      focus_group_accepts_boundary_key(Key) const noexcept override;

  void focus_group_select() override;
  void focus_group_select_guarded(const std::function<bool()>& allowed) override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  void paint(PaintContext &context) const override;

private:
  [[nodiscard]] bool source_valid() const;
  [[nodiscard]] VisualState current_visual_state(bool focused) const;

  [[nodiscard]] VisualState
  current_visual_state(ComponentAvailability availability, bool focused) const;

  void sync_state_invalidation(ComponentAvailability availability,
                               bool focused) const noexcept;

  [[nodiscard]] ResolvedRadioStyle resolved_style(bool focused) const;

  [[nodiscard]] ResolvedRadioStyle
  resolved_style(ComponentAvailability availability, bool focused) const;

  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const override;

  [[nodiscard]] bool availability_change_affects_paint(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const noexcept override;

  template <class Context>
  static void invalidate_transition(const ResolvedRadioStyle &before,
                                    const ResolvedRadioStyle &after,
                                    Context &context);

  std::shared_ptr<RadioGroupToken> group_;
  std::string label_;
  std::function<std::uint64_t()> selection_revision_;
  IsSelected is_selected_;
  IsValid is_valid_;
  Select select_;
  std::function<void(const std::function<bool()>&)> guarded_select_;
  std::function<bool()> mutation_guard_;
  Observe observe_;
  RadioStyle style_;
  std::shared_ptr<void> subscription_;
  std::shared_ptr<RadioStateInvalidation> state_invalidation_;
  PressActivationState interaction_;
  std::function<void()> release_pointer_;
  bool focused_{};
};

} // namespace detail

template <class T> class RadioGroup {
public:
  explicit RadioGroup(Binding<T> selected)
      : selected_(std::move(selected)),
        token_(std::make_shared<detail::RadioGroupToken>()),
        registry_(std::make_shared<detail::RadioOptionRegistry<T>>()) {}

  explicit RadioGroup(State<T> &selected) : RadioGroup(selected.binding()) {}

private:
  template <class U> friend class RadioButton;

  Binding<T> selected_;
  std::shared_ptr<detail::RadioGroupToken> token_;
  std::shared_ptr<detail::RadioOptionRegistry<T>> registry_;
};

template <class T> class RadioButton {
public:
  RadioButton(const RadioGroup<T> &group, T value, std::string label)
      : selected_(group.selected_), token_(group.token_),
        registry_(group.registry_), value_(std::move(value)),
        label_(std::move(label)) {}

  RadioButton &&style(RadioStyle value) && {
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
    detail::RadioButtonComponent::IsSelected is_selected = [selected,
                                                            value_identity] {
      return selected.get() == *value_identity;
    };
    detail::RadioButtonComponent::Select select = [selected,
                                                   value_identity]() mutable {
      if (selected.valid())
        selected.set(*value_identity);
    };
    auto guarded_select = [selected, value_identity](const std::function<bool()>& allowed) mutable {
      const auto revision = selected.revision();
      T candidate = *value_identity;
      if (!selected.valid() || (allowed && !allowed())) return;
      selected.set_if(std::move(candidate), [selected, revision, allowed] {
        return selected.valid() && selected.revision() == revision && (!allowed || allowed());
      });
    };
    detail::RadioButtonComponent::Observe observe =
        [selected](
            std::function<void()> invalidate) mutable -> std::shared_ptr<void> {
      auto subscription = selected.observe(
          [invalidate = std::move(invalidate)](const T &) { invalidate(); });
      return std::make_shared<typename Binding<T>::Subscription>(
          std::move(subscription));
    };

    detail::RadioButtonComponent::IsValid is_valid = [selected] {
      return selected.valid();
    };
    return Spec{
        [token = std::move(token), label = std::move(label),
         is_selected = std::move(is_selected), select = std::move(select),
         guarded_select = std::move(guarded_select),
         revision_reader = [selected] { return selected.revision(); },
         observe = std::move(observe), is_valid = std::move(is_valid),
         style = std::move(style)]() mutable {
          auto component = std::make_unique<detail::RadioButtonComponent>(
              std::move(token), std::move(label), std::move(is_selected),
              std::move(select), std::move(observe), std::move(style),
              std::move(is_valid));
          component->set_selection_revision(std::move(revision_reader));
          component->set_guarded_select(std::move(guarded_select));
          return component;
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
RadioButton(const RadioGroup<T> &, T, std::string) -> RadioButton<T>;

} // namespace ui
