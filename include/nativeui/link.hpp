#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ui {
struct LinkStyle {
  std::optional<TextStyle> text;
  std::optional<Color> color;
  std::optional<Color> hovered_color;
  std::optional<Color> disabled_color;
  std::optional<Color> focus_ring;
  bool underline{};
  bool hovered_underline{true};
  double focus_padding{2.0};
  double focus_ring_width{1.0};
};
namespace detail {
struct LinkInteraction;
class LinkComponent final : public Component, public ThemeBinding {
public:
  using NavigateCallback = std::function<void(const std::string &)>;
  LinkComponent(std::string label, std::string destination,
                NavigateCallback navigate, LinkStyle style, bool wrap);
  [[nodiscard]] bool focusable() const noexcept override;
  [[nodiscard]] bool roving_focus_target() const noexcept override;
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] ChildMetrics
  measure_constrained(const Constraints &,
                      const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  void deactivate(LifecycleContext &) override;
  void focus_changed(bool, FocusContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &) const override;

private:
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &) noexcept override;
  [[nodiscard]] TextStyle text_style() const;
  std::string label_;
  std::string destination_;
  NavigateCallback navigate_;
  LinkStyle style_;
  bool wrap_{};
  bool focused_{};
  std::shared_ptr<LinkInteraction> interaction_;
};
} // namespace detail
class Link {
public:
  using NavigateCallback = detail::LinkComponent::NavigateCallback;
  Link(std::string label, std::string destination, NavigateCallback navigate);
  Link &&style(LinkStyle value) &&;
  Link &&wrap(bool value = true) &&;
  Spec spec() &&;

private:
  std::string label_;
  std::string destination_;
  NavigateCallback navigate_;
  LinkStyle style_;
  bool wrap_{};
};
} // namespace ui
