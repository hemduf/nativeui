#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/group_button_style.hpp>
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

#include <nativeui/style.hpp>

namespace ui {

enum class ButtonVariant { Standard, Primary, Toolbar };

enum class ButtonVisualState {
  Normal,
  Hover,
  Pressed,
  Focused,
  Disabled,
};

namespace detail {
class ButtonComponent final : public Component,
                              public ThemeBinding,
                              public GroupButtonStyleTarget {
public:
  void bind_group_style(
      std::shared_ptr<const GroupButtonStyle>) noexcept override;
  using ActivateCallback = std::function<void()>;

  ButtonComponent(std::string label, ActivateCallback on_activate,
                  ButtonStyle style);
  ButtonComponent(std::string label, ActivateCallback on_activate,
                  ButtonStyle style, ButtonVariant variant, bool has_content);

  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool roving_focus_target() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] bool clips_children() const noexcept override;
  [[nodiscard]] bool allows_child_interaction() const noexcept override;
  [[nodiscard]] Constraints child_constraints(const Constraints &constraints,
                                              std::size_t index,
                                              std::size_t count) const override;
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override;

  void focus_changed(bool focused, FocusContext &context) override;

  void deactivate(LifecycleContext &context) override;

  EventResult input(const InputEvent &event, InputContext &context) override;

  [[nodiscard]] ButtonVisualState visual_state(bool focused) const noexcept;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &context) const override;

private:
  struct PresentationSignature {
    Color fill{};
    Color border{};
    Color text{};
    float border_width{};
    float corner_radius{};
    float minimum_width{};
    float control_height{};
    float horizontal_padding{};
    float text_size{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string_view font_family;
    const std::vector<std::string> *fallback_families{};
  };

  static void apply_patch(PresentationSignature &target,
                          const ButtonStylePatch &patch) noexcept;

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

  [[nodiscard]] bool availability_change_affects_paint(
      const ComponentAvailability &before,
      const ComponentAvailability &after) const noexcept override;

  [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept;

  [[nodiscard]] VisualState
  current_visual_state(ComponentAvailability availability,
                       bool focused) const noexcept;

  [[nodiscard]] ResolvedButtonStyle resolved_style(bool focused) const;

  std::string label_;
  ActivateCallback on_activate_;
  ButtonStyle style_;
  std::shared_ptr<const GroupButtonStyle> group_style_;
  PressActivationState interaction_;
  ButtonVariant variant_{ButtonVariant::Standard};
  bool has_content_{};
  bool focused_{};
};
} // namespace detail

class Button {
public:
  using ActivateCallback = detail::ButtonComponent::ActivateCallback;

  Button(std::string label, ActivateCallback on_activate);

  Button &&style(ButtonStyle value) &&;
  Button &&variant(ButtonVariant value) &&;
  Button &&content(Spec value) &&;

  Spec spec() &&;

private:
  std::string label_;
  ActivateCallback on_activate_;
  ButtonStyle style_;
  ButtonVariant variant_{ButtonVariant::Standard};
  std::vector<Spec> children_;
};

} // namespace ui
