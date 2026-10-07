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

#include <nativeui/slider.hpp>

namespace ui {

struct RangeValue {
  float low{};
  float high{1.0f};

  friend bool operator==(const RangeValue &, const RangeValue &) = default;
};

namespace detail {

enum class RangeSliderThumb {
  Lower,
  Upper,
};

class RangeSliderComponent final : public Component, public ThemeBinding {
public:
  RangeSliderComponent(Binding<RangeValue> state, float minimum, float maximum,
                       float step, SliderOrientation orientation,
                       SliderStyle style);

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &context) override;

  void unmount(LifecycleContext &) override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  [[nodiscard]] SliderVisualState visual_state(bool focused) const noexcept;

  void paint(PaintContext &context) const override;

private:
  template <class Context>
  void invalidate_style_transition(const ResolvedSliderStyle &before,
                                   const ResolvedSliderStyle &after,
                                   Context &context,
                                   bool force_paint = false) const;

  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const override;

  [[nodiscard]] VisualState
  current_visual_state(ComponentAvailability availability,
                       bool focused) const noexcept;

  [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept;

  [[nodiscard]] ResolvedSliderStyle
  resolved_style(ComponentAvailability availability, bool focused) const;

  [[nodiscard]] ResolvedSliderStyle resolved_style(bool focused) const;

  [[nodiscard]] bool mutating_input(const InputEvent &event) const noexcept;

  [[nodiscard]] RangeValue
  effective_range(const RangeValue &value) const noexcept;

  [[nodiscard]] float
  raw_value_from_pointer(const InputEvent &event,
                         const InputContext &context) const noexcept;

  [[nodiscard]] RangeSliderThumb
  choose_thumb(float raw_target, const RangeValue &effective) const noexcept;

  [[nodiscard]] RangeValue edited_value(double raw_target) const noexcept;

  void commit_value(const RangeValue &value);

  EventResult begin_drag(const InputEvent &event, InputContext &context);

  EventResult pointer_move(const InputEvent &event, InputContext &context);

  EventResult update_drag(const InputEvent &event, InputContext &context,
                          bool release, const ResolvedSliderStyle &before = {});

  EventResult key_down(const InputEvent &event, InputContext &);

  Binding<RangeValue> state_;
  SliderDomain domain_;
  SliderOrientation orientation_{SliderOrientation::Horizontal};
  SliderStyle style_;
  Binding<RangeValue>::Subscription subscription_;
  RangeSliderThumb active_thumb_{RangeSliderThumb::Lower};
  bool has_active_thumb_{};
  bool focused_{};
  bool dragging_{};
  bool hovered_{};
  bool enter_pressed_{};
};
} // namespace detail

class RangeSlider {
public:
  explicit RangeSlider(Binding<RangeValue> state);
  explicit RangeSlider(State<RangeValue> &state);

  RangeSlider &&range(float minimum, float maximum) &&;

  RangeSlider &&step(float value) &&;

  RangeSlider &&orientation(SliderOrientation value) &&;

  RangeSlider &&style(SliderStyle value) &&;

  Spec spec() &&;

private:
  Binding<RangeValue> state_;
  float minimum_{0.0f};
  float maximum_{1.0f};
  float step_{};
  SliderOrientation orientation_{SliderOrientation::Horizontal};
  SliderStyle style_;
};

} // namespace ui
