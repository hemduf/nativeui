#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

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

#include <nativeui/detail/checkbox_radio_helpers.hpp>

namespace ui {

namespace detail {
class CheckboxComponent final : public Component, public ThemeBinding {
public:
  CheckboxComponent(Binding<bool> state, std::string label,
                    CheckboxStyle style);

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &context) override;

  void unmount(LifecycleContext &) override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  EventResult semantic_action(SemanticAction action, InputContext &context) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &context) const override;

private:
  [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept;

  [[nodiscard]] VisualState
  current_visual_state(ComponentAvailability availability,
                       bool focused) const noexcept;

  void sync_state_invalidation(ComponentAvailability availability,
                               bool focused) const noexcept;

  [[nodiscard]] ResolvedCheckboxStyle resolved_style(bool focused) const;

  [[nodiscard]] ResolvedCheckboxStyle
  resolved_style(ComponentAvailability availability, bool focused) const;

  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const override;

  [[nodiscard]] bool availability_change_affects_paint(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const noexcept override;

  template <class Context>
  static void invalidate_transition(const ResolvedCheckboxStyle &before,
                                    const ResolvedCheckboxStyle &after,
                                    Context &context);

  Binding<bool> state_;
  std::string label_;
  CheckboxStyle style_;
  Binding<bool>::Subscription subscription_;
  std::shared_ptr<CheckboxStateInvalidation> state_invalidation_;
  PressActivationState interaction_;
  bool focused_{};
};
} // namespace detail

class Checkbox {
public:
  Checkbox(Binding<bool> state, std::string label);

  Checkbox(State<bool> &state, std::string label);

  Checkbox &&style(CheckboxStyle value) &&;

  Spec spec() &&;

private:
  Binding<bool> state_;
  std::string label_;
  CheckboxStyle style_;
};

} // namespace ui
