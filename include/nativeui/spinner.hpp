#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ui {
struct SpinnerStyle {
  std::optional<Color> color;
  double thickness_ratio{0.09};
  std::optional<double> size;
};
namespace detail {
struct SpinnerMotion;
class SpinnerComponent final : public Component, public ThemeBinding {
public:
  SpinnerComponent(std::string label, std::optional<Binding<bool>> active,
                   std::optional<double> size, bool reduced_motion,
                   SpinnerStyle style);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] Size
  minimum_size(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &context) override;
  void activate(LifecycleContext &context) override;
  void deactivate(LifecycleContext &context) override;
  void unmount(LifecycleContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &context) const override;

private:
  void layout_committed(Rect previous, Rect current) noexcept override;
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override;
  [[nodiscard]] double diameter() const noexcept;
  std::string label_;
  std::optional<double> size_;
  SpinnerStyle style_;
  std::shared_ptr<SpinnerMotion> motion_;
  Binding<bool>::Subscription subscription_;
};
} // namespace detail
class Spinner {
public:
  explicit Spinner(std::string label = {});
  Spinner &&active(Binding<bool> value) &&;
  Spinner &&active(State<bool> &value) &&;
  Spinner &&size(double value) &&;
  Spinner &&reduced_motion(bool value = true) &&;
  Spinner &&style(SpinnerStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  std::optional<Binding<bool>> active_;
  std::optional<double> size_;
  bool reduced_motion_{};
  SpinnerStyle style_;
};
} // namespace ui
