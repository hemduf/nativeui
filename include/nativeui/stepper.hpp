#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ui {
struct StepperStylePatch {
  std::optional<Color> fill;
  std::optional<Color> arrow;
  std::optional<Color> border;
  std::optional<double> width;
  std::optional<double> height;
  std::optional<double> border_width;
  std::optional<double> corner_radius;
};
struct StepperStyle {
  StepperStylePatch base;
  StepperStylePatch hovered;
  StepperStylePatch pressed;
  StepperStylePatch focused;
  StepperStylePatch disabled;
  StepperStylePatch read_only;
};
namespace detail {
struct StepperRepeat;
struct StepperAccess;
class StepperComponent final : public Component, public ThemeBinding {
public:
  StepperComponent(Binding<double> value, std::string label, double minimum,
                   double maximum, double step, StepperStyle style);
  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override;
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &) override;
  void activate(LifecycleContext &) override;
  void deactivate(LifecycleContext &) override;
  void unmount(LifecycleContext &) override;
  void focus_changed(bool, FocusContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &) const override;

private:
  friend struct StepperAccess;
  bool focusable_override_{true};
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &) noexcept override;
  void layout_committed(Rect, Rect) noexcept override;
  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &,
      const ComponentAvailability &) const override;
  struct Presentation {
    Color fill;
    Color arrow;
    Color border;
    float width;
    float height;
    float border_width;
    float corner_radius;
  };
  [[nodiscard]] Presentation presentation(ComponentAvailability,
                                          int cell) const noexcept;
  [[nodiscard]] bool interaction_affects_layout() const noexcept;
  std::string label_;
  StepperStyle style_;
  std::shared_ptr<StepperRepeat> repeat_;
  Binding<double>::Subscription subscription_;
  int hovered_{};
  bool focused_{};
};
} // namespace detail
class Stepper {
public:
  explicit Stepper(Binding<double> value);
  explicit Stepper(State<double> &value);
  Stepper &&label(std::string value) &&;
  Stepper &&range(double minimum, double maximum) &&;
  Stepper &&step(double value) &&;
  Stepper &&style(StepperStyle value) &&;
  Spec spec() &&;

private:
  Binding<double> value_;
  std::string label_;
  double minimum_{};
  double maximum_{100.0};
  double step_{1.0};
  StepperStyle style_;
};
} // namespace ui
