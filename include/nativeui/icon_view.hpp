#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>
#include <nativeui/svg.hpp>

#include <optional>
#include <string>

namespace ui {
namespace detail {
class IconViewComponent final : public Component, public ThemeBinding {
public:
  IconViewComponent(SvgIcon source, std::optional<Binding<SvgIcon>> binding,
                    std::optional<double> height, std::optional<Color> color,
                    bool monochrome, std::string alt, bool decorative);
  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;
  void mount(MountContext &) override;
  void unmount(LifecycleContext &) override;
  EventResult input(const InputEvent &, InputContext &) override;
  void paint(PaintContext &) const override;

private:
  [[nodiscard]] SvgIcon current_source() const;
  [[nodiscard]] double height() const noexcept;
  SvgIcon source_;
  std::optional<Binding<SvgIcon>> binding_;
  std::optional<double> height_;
  std::optional<Color> color_;
  bool monochrome_{true};
  std::string alt_;
  bool decorative_{true};
  Binding<SvgIcon>::Subscription subscription_;
};
} // namespace detail
class IconView {
public:
  explicit IconView(SvgIcon value);
  explicit IconView(Binding<SvgIcon> value);
  explicit IconView(State<SvgIcon> &value);
  IconView &&size(double height) &&;
  IconView &&color(Color value) &&;
  IconView &&monochrome(bool value = true) &&;
  IconView &&alt(std::string value) &&;
  IconView &&decorative(bool value = true) &&;
  Spec spec() &&;

private:
  SvgIcon source_;
  std::optional<Binding<SvgIcon>> binding_;
  std::optional<double> height_;
  std::optional<Color> color_;
  bool monochrome_{true};
  std::string alt_;
  bool decorative_{true};
};
} // namespace ui
