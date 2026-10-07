#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include <optional>

namespace ui {
enum class DividerOrientation { Horizontal, Vertical };

namespace detail {
class DividerComponent final : public Component, public ThemeBinding {
public:
  DividerComponent(DividerOrientation orientation, double thickness,
                   std::optional<Color> color);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] Size
  minimum_size(const std::vector<ChildMetrics> &) const override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &context) const override;

private:
  DividerOrientation orientation_;
  double thickness_;
  std::optional<Color> color_;
};
} // namespace detail

class Divider {
public:
  explicit Divider(DividerOrientation value = DividerOrientation::Horizontal);
  Divider &&thickness(double value) &&;
  Divider &&color(Color value) &&;
  Spec spec() &&;

private:
  DividerOrientation orientation_;
  double thickness_{1.0};
  std::optional<Color> color_;
};
} // namespace ui
