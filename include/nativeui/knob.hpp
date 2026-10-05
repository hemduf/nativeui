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
#include <cstdint>
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

#include <nativeui/detail/widget_text_paint.hpp>

namespace ui {

class KnobComponent final : public Component {
public:
  KnobComponent(std::string label, Binding<float> state, float minimum,
                float maximum);

  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override;
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &ctx) override;
  void unmount(LifecycleContext &ctx) override;
  void deactivate(LifecycleContext &ctx) override;
  void focus_changed(bool focused, FocusContext &ctx) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  EventResult input(const InputEvent &event, InputContext &ctx) override;

  void paint(PaintContext &p) const override;

private:
  struct Observation {
    DragGesture gesture;
    std::uint64_t generation{};
    std::function<void()> release_pointer;
    bool active{};
    bool dragging{};
    bool cancel_requested{};
    bool writing{};
    float expected{};
    float value{};
  };

  [[nodiscard]] float effective_value(float value) const noexcept;
  void publish(double value, InputContext &ctx);

  std::string label_;
  Binding<float> state_;
  float value_{};
  float minimum_{};
  float maximum_{1.0f};
  float drag_start_value_{};
  Binding<float>::Subscription subscription_;
  std::shared_ptr<Observation> observation_;
};

class Knob {
public:
  Knob(std::string label, Binding<float> state);

  Knob(std::string label, State<float> &state);

  Knob &&range(float minimum, float maximum) &&;

  Spec spec() &&;

private:
  std::string label_;
  Binding<float> state_;
  float minimum_{0.0f};
  float maximum_{1.0f};
};

} // namespace ui
