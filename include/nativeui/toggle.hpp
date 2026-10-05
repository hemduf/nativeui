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

#include <nativeui/detail/widget_text_paint.hpp>
#include <nativeui/toggle_style.hpp>

namespace ui {

class ToggleComponent final : public Component, public detail::ThemeBinding {
public:
  ToggleComponent(std::string label, Binding<bool> state, ToggleStyle style);

  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &ctx) override;

  void unmount(LifecycleContext &) override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  EventResult input(const InputEvent &event, InputContext &ctx) override;

  EventResult semantic_action(SemanticAction action, InputContext &context) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &p) const override;

private:
  struct PresentationSignature {
    Color fill{};
    Color border{};
    Color text{};
    Color track{};
    Color thumb{};
    float border_width{};
    float corner_radius{};
    float control_width{};
    float control_height{};
    float leading_padding{};
    float trailing_padding{};
    float track_width{};
    float track_height{};
    float thumb_diameter{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string_view font_family;
    const std::vector<std::string> *fallback_families{};
  };

  [[nodiscard]] static bool
  patch_affects_layout(const ToggleStylePatch &patch) noexcept;

  static void apply_patch(PresentationSignature &target,
                          const ToggleStylePatch &patch) noexcept;

  [[nodiscard]] PresentationSignature
  presentation_signature(bool focused) const noexcept;

  [[nodiscard]] PresentationSignature
  presentation_signature(ComponentAvailability availability,
                         bool focused) const noexcept;

  [[nodiscard]] static bool same_color(Color lhs, Color rhs) noexcept;

  [[nodiscard]] static bool same_fallbacks(const PresentationSignature &lhs,
                                           const PresentationSignature &rhs);

  [[nodiscard]] static bool same_layout(const PresentationSignature &lhs,
                                        const PresentationSignature &rhs);

  [[nodiscard]] static bool same_presentation(const PresentationSignature &lhs,
                                              const PresentationSignature &rhs);

  template <class Context>
  static void invalidate_style_transition(const PresentationSignature &before,
                                          const PresentationSignature &after,
                                          Context &context);

  [[nodiscard]] bool availability_change_affects_layout(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const override;

  [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept;

  [[nodiscard]] VisualState
  current_visual_state(ComponentAvailability availability,
                       bool focused) const noexcept;

  [[nodiscard]] ResolvedToggleStyle resolved_style(bool focused) const;

  std::string label_;
  Binding<bool> state_;
  ToggleStyle style_;
  detail::PressActivationState interaction_;
  bool focused_{};
  bool space_pressed_{};
  bool enter_pressed_{};
  Binding<bool>::Subscription subscription_;
};

class Toggle {
public:
  Toggle(std::string label, Binding<bool> state);

  Toggle(std::string label, State<bool> &state);

  Toggle &&style(ToggleStyle value) &&;

  Spec spec() &&;

private:
  std::string label_;
  Binding<bool> state_;
  ToggleStyle style_;
};

} // namespace ui
