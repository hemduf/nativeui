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

#include <nativeui/progress_style.hpp>
#include <stdexcept>

namespace ui {
enum class ProgressOrientation {
  Horizontal,
  Vertical,
};

struct MeterLevels {
  double warning{};
  double critical{};
};

namespace detail {
[[nodiscard]] Color meter_fill_color(float value, Color normal,
                                     const std::optional<MeterLevels> &levels,
                                     Color warning, Color critical) noexcept;
[[nodiscard]] std::string
meter_level_description(float value, const std::optional<MeterLevels> &levels);
struct ProgressMotion;

class BoundedDisplayDomain final {
public:
  BoundedDisplayDomain(float minimum, float maximum);

  [[nodiscard]] float minimum() const noexcept;
  [[nodiscard]] float maximum() const noexcept;

  [[nodiscard]] float effective(float value) const noexcept;

  [[nodiscard]] float fraction(float value) const noexcept;

  [[nodiscard]] Rect fill_rect(Rect bounds, float value,
                               ProgressOrientation orientation) const noexcept;

private:
  float minimum_{};
  float maximum_{};
};

class BoundedDisplayComponent final : public Component, public ThemeBinding {
public:
  using Formatter = std::function<std::string(float)>;

  BoundedDisplayComponent(State<float> &state, float minimum, float maximum,
                          ProgressOrientation orientation, Formatter formatter,
                          ProgressBarStyle style);

  BoundedDisplayComponent(State<float> &state, float minimum, float maximum,
                          ProgressOrientation orientation, Formatter formatter,
                          MeterStyle style);

  BoundedDisplayComponent(Binding<float> state, float minimum, float maximum,
                          ProgressOrientation orientation, Formatter formatter,
                          ProgressBarStyle style, bool reversed = false,
                          bool indeterminate = false,
                          bool reduced_motion = false);

  BoundedDisplayComponent(Binding<float> state, float minimum, float maximum,
                          ProgressOrientation orientation, Formatter formatter,
                          MeterStyle style,
                          std::optional<MeterLevels> levels = {},
                          Color warning = {1.0f, 0.65f, 0.0f, 1.0f},
                          Color critical = {0.85f, 0.15f, 0.15f, 1.0f});

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
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
      const ComponentAvailability &before,
      const ComponentAvailability &after) noexcept override;
  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const override;

  [[nodiscard]] Size
  preferred_size(const ResolvedProgressStyle &resolved) const noexcept;

  [[nodiscard]] static VisualState
  visual_state(const ComponentAvailability &availability) noexcept;

  [[nodiscard]] VisualState current_visual_state() const noexcept;

  [[nodiscard]] ResolvedProgressStyle
  resolved_style(const ComponentAvailability &availability) const;

  [[nodiscard]] ResolvedProgressStyle resolved_style() const;

  Binding<float> state_;
  BoundedDisplayDomain domain_;
  ProgressOrientation orientation_{ProgressOrientation::Horizontal};
  Formatter formatter_;
  Binding<float>::Subscription subscription_;
  ProgressBarStyle progress_style_;
  MeterStyle meter_style_;
  bool meter_{};
  bool reversed_{};
  bool indeterminate_{};
  std::shared_ptr<ProgressMotion> motion_;
  std::optional<MeterLevels> levels_;
  Color warning_{1.0f, 0.65f, 0.0f, 1.0f};
  Color critical_{0.85f, 0.15f, 0.15f, 1.0f};
};
} // namespace detail
} // namespace ui
