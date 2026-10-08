#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>

#include <optional>
#include <string>

namespace ui {
struct BadgeStyle {
  BadgeStyle();
  TextStyle text;
  Color background;
  double horizontal_padding{6.0};
  double vertical_padding{1.0};
  double minimum_width{};
};

namespace detail {
class BadgeComponent final : public Component, public ThemeBinding {
public:
  BadgeComponent(std::string text, std::optional<Binding<std::string>> source,
                 std::optional<BadgeStyle> style);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &context) override;
  void unmount(LifecycleContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &context) const override;

private:
  [[nodiscard]] std::string current_text() const;
  [[nodiscard]] BadgeStyle resolved_style() const;
  std::string text_;
  std::optional<Binding<std::string>> source_;
  std::optional<BadgeStyle> style_;
  Binding<std::string>::Subscription subscription_;
};
} // namespace detail

class Badge {
public:
  explicit Badge(std::string text);
  explicit Badge(Binding<std::string> text);
  explicit Badge(State<std::string> &text);
  Badge &&style(BadgeStyle value) &&;
  Spec spec() &&;

private:
  std::string text_;
  std::optional<Binding<std::string>> source_;
  std::optional<BadgeStyle> style_;
};
} // namespace ui
