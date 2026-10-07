#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/edit.hpp>
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

#include <nativeui/detail/slider_value.hpp>
#include <nativeui/slider_style.hpp>

namespace ui {

enum class SliderOrientation {
  Horizontal,
  Vertical,
};

enum class SliderVisualState {
  Normal,
  Hover,
  Pressed,
  Focused,
  Disabled,
  ReadOnly,
};

namespace detail {

[[nodiscard]] constexpr SliderVisualState
slider_visual_state(bool enabled, bool read_only, bool hovered, bool pressed,
                    bool focused) noexcept {
  if (!enabled)
    return SliderVisualState::Disabled;
  if (read_only)
    return SliderVisualState::ReadOnly;
  if (pressed)
    return SliderVisualState::Pressed;
  if (hovered)
    return SliderVisualState::Hover;
  if (focused)
    return SliderVisualState::Focused;
  return SliderVisualState::Normal;
}

[[nodiscard]] constexpr Color
slider_active_color(SliderVisualState state) noexcept {
  switch (state) {
  case SliderVisualState::Hover:
    return colors::caret;
  case SliderVisualState::Pressed:
    return colors::text;
  case SliderVisualState::Disabled:
    return colors::textMuted;
  case SliderVisualState::ReadOnly:
    return colors::track;
  case SliderVisualState::Normal:
  case SliderVisualState::Focused:
    return colors::accent;
  }
  return colors::accent;
}

[[nodiscard]] constexpr Color
slider_active_color(SliderVisualState state,
                    const ThemePalette &palette) noexcept {
  switch (state) {
  case SliderVisualState::Hover:
    return palette.active_highlight;
  case SliderVisualState::Pressed:
    return palette.text;
  case SliderVisualState::Disabled:
    return palette.disabled;
  case SliderVisualState::ReadOnly:
    return palette.track;
  case SliderVisualState::Normal:
  case SliderVisualState::Focused:
    return palette.accent;
  }
  return palette.accent;
}

// The visual thumb travel is also the pointer-editing domain. Keeping both
// directions in one helper prevents paint and hit/value mapping from drifting
// when track insets or reserved formatter space change.
struct SliderTrackAxis final {
  float start{};
  float end{};
  bool inverted{};

  [[nodiscard]] float position(float fraction) const noexcept;

  [[nodiscard]] float fraction(float coordinate) const noexcept;
};

[[nodiscard]] SliderTrackAxis
slider_track_axis(Rect bounds, SliderOrientation orientation,
                  bool has_formatter = false) noexcept;

class SliderComponent final : public Component, public ThemeBinding {
public:
  using Formatter = std::function<std::string(float)>;

  SliderComponent(Binding<float> state, float minimum, float maximum,
                  float step, SliderOrientation orientation,
                  Formatter formatter, SliderStyle style);
  SliderComponent(Binding<float> state, float minimum, float maximum,
                  float step, SliderOrientation orientation,
                  Formatter formatter, SliderStyle style,
                  EditCallbacks<float> callbacks, bool wheel_enabled = false);

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &context) override;

  void unmount(LifecycleContext &) override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  [[nodiscard]] SliderVisualState visual_state(bool focused) const noexcept;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &context) const override;

private:
  struct Interaction {
    SliderComponent *owner{};
    std::uint64_t generation{};
    std::function<void()> release_pointer;
  };

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

  [[nodiscard]] float
  value_from_pointer(const InputEvent &event,
                     const InputContext &context) const noexcept;

  EventResult begin_drag(const InputEvent &event, InputContext &context);

  EventResult pointer_move(const InputEvent &event, InputContext &context);

  EventResult update_drag(const InputEvent &event, InputContext &context,
                          bool release, const ResolvedSliderStyle &before = {});

  EventResult key_down(const InputEvent &event, InputContext &);

  Binding<float> state_;
  std::shared_ptr<EditSession<float>> edit_;
  std::shared_ptr<Interaction> interaction_;
  bool wheel_enabled_{};
  SliderDomain domain_;
  SliderOrientation orientation_{SliderOrientation::Horizontal};
  Formatter formatter_;
  SliderStyle style_;
  Binding<float>::Subscription subscription_;
  bool focused_{};
  bool dragging_{};
  bool hovered_{};
};
} // namespace detail

class Slider {
public:
  using Formatter = detail::SliderComponent::Formatter;

  explicit Slider(Binding<float> state);
  explicit Slider(State<float> &state);

  Slider &&range(float minimum, float maximum) &&;

  Slider &&step(float value) &&;

  Slider &&orientation(SliderOrientation value) &&;

  Slider &&formatter(Formatter value) &&;

  Slider &&style(SliderStyle value) &&;

  Slider &&on_edit(EditCallbacks<float> callbacks) &&;

  Slider &&wheel_enabled(bool value = true) &&;

  Spec spec() &&;

private:
  Binding<float> state_;
  float minimum_{0.0f};
  float maximum_{1.0f};
  float step_{};
  SliderOrientation orientation_{SliderOrientation::Horizontal};
  Formatter formatter_;
  SliderStyle style_;
  EditCallbacks<float> callbacks_{};
  bool wheel_enabled_{};
};

} // namespace ui
