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

struct ToggleButtonStyle : ButtonStyle {
  ButtonStylePatch selected;
};

enum class ToggleButtonVisualState {
  Normal,
  Hover,
  Pressed,
  Focused,
  Disabled,
};

namespace detail {
class ToggleButtonComponent final : public Component,
                                    public ThemeBinding,
                                    public GroupButtonStyleTarget {
public:
  void bind_group_style(
      std::shared_ptr<const GroupButtonStyle>) noexcept override;
  ToggleButtonComponent(std::string label, Binding<bool> pressed,
                        ToggleButtonStyle style, bool has_content);
  void mount(MountContext &context) override;
  void unmount(LifecycleContext &) override;
  [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override;

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

  [[nodiscard]] ToggleButtonVisualState
  visual_state(bool focused) const noexcept;

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
  Binding<bool> pressed_;
  Binding<bool>::Subscription subscription_;
  ToggleButtonStyle style_;
  std::shared_ptr<const GroupButtonStyle> group_style_;
  std::function<void()> release_pointer_;
  PressActivationState interaction_;
  bool has_content_{};
  bool focused_{};
};
} // namespace detail

class ToggleButton {
public:
  ToggleButton(std::string label, Binding<bool> pressed);
  ToggleButton(std::string label, State<bool> &pressed);
  ToggleButton &&style(ToggleButtonStyle value) &&;
  ToggleButton &&content(Spec value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<bool> pressed_;
  ToggleButtonStyle style_;
  std::vector<Spec> children_;
};
} // namespace ui
